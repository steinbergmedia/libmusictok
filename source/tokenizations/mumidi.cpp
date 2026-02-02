//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/mumidi.cpp
// Created by  : Steinberg, 10/2025
// Description : MuMIDI tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/mumidi.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
MuMIDI::MuMIDI(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kMuMIDI, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
MuMIDI::MuMIDI(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kMuMIDI, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void MuMIDI::tweakConfigBeforeCreatingVoc()
{
    config.useRests                  = false;
    config.useTimeSignatures         = false;
    config.useSustainPedals          = false;
    config.usePitchBends             = false;
    config.usePrograms               = true;
    config.usePitchIntervals         = true;
    config.oneTokenStreamForPrograms = true;
    config.programChanges            = false;

    disableAttributeControls();

    // Durations are enabled for all programs or none
    bool hasProgamNotInDurationSet = std::any_of(config.programs.begin(), config.programs.end(), [&](int p) {
        return config.useNoteDurationPrograms.find(p) == config.useNoteDurationPrograms.end();
    });
    if (hasProgamNotInDurationSet)
    {
        config.useNoteDurationPrograms = config.programs;
        if (verbose)
        {
            std::clog << "Setting note duration programs to `tokenizer.config.programs`. "
                         "MuMIDI only allows to use note duration tokens for either all "
                         "programs or none."
                      << std::endl;
        }
    }

    if (config.additionalParams.find("max_bar_embedding") == config.additionalParams.end())
    {
        config.additionalParams["max_bar_embedding"] = 60;
    }

    if (config.useVelocities)
        vocabTypesIdx["Velocity"] = -1;

    if (config.usingNoteDurationTokens())
    {
        vocabTypesIdx["Duration"] = -1;
        if (config.useVelocities)
            vocabTypesIdx["Velocity"] = -2;
    }

    if (config.useChords)
        vocabTypesIdx["Chord"] = 0;

    if (config.useRests)
        vocabTypesIdx["Rest"] = 0;

    if (config.useTempos)
        vocabTypesIdx["Tempo"] = -3;
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MuMIDI::scoreToTokens(
    const ScoreType &score_, const AttributeControlsIndexesType &attributeControlIndexes) const
{
    ScoreType score           = score_;
    std::vector<int> barTicks = libmusictokUtils::getBarsTicks(score, true);
    int maxBarEmbedding       = config.additionalParams.at("max_bar_embedding").get<int>();
    if (maxBarEmbedding < barTicks.size())
    {
        score.clip_inplace(0, barTicks.at(maxBarEmbedding));
        if (verbose)
        {
            std::clog << "MuMIDI cannot entirely tokenize this file "
                         "as it contains "
                      << std::to_string(barTicks.size()) << "bars whereas the limit of the tokenizer is "
                      << std::to_string(maxBarEmbedding) << ". It is therefore clipped to "
                      << std::to_string(maxBarEmbedding) << " bars." << std::endl;
        }
    }

    // Convert each track to tokens (except first po to track time)
    std::vector<std::pair<int, int>> ticksPerBeat;
    if (config.useChords)
    {
        ticksPerBeat = libmusictokUtils::getScoreTicksPerBeat(score);
    }
    // else keep empty

    std::vector<std::vector<Event>> noteTokens;
    for (std::shared_ptr<TrackType> track : *score.tracks)
    {
        if (config.programs.find(track->program) != config.programs.end())
        {
            std::vector<std::vector<Event>> trackTokens = trackToTokens(track, ticksPerBeat);
            noteTokens.insert(noteTokens.end(), trackTokens.begin(), trackTokens.end());
        }
    }

    std::stable_sort(noteTokens.begin(), noteTokens.end(),
                     [this](const std::vector<Event> &a, const std::vector<Event> &b) {
                         int timea = a.at(0).time;
                         int timeb = b.at(0).time;
                         int desca = std::get<int>(a.at(0).desc);
                         int descb = std::get<int>(b.at(0).desc);
                         if (timea != timeb)
                             return timea < timeb;
                         return desca < descb;
                     });

    double ticksPerSample =
        static_cast<double>(score.ticks_per_quarter) / static_cast<double>(config.maxNumPosPerBeat());
    int ticksPerBar = score.ticks_per_quarter * 4;
    std::vector<std::vector<std::string>> tokens;

    int currentTick     = -1;
    int currentBar      = -1;
    int currentPos      = -1;
    int currentTrack    = -2;
    int currentTempoIdx = 0;
    double currentTempo = std::stod(libmusictokUtils::roundToTwoDecimals(score.tempos->at(currentTempoIdx).qpm()));

    for (const std::vector<Event> &noteToken : noteTokens)
    {
        // Update tempo values currentTempo
        // if the current tempo is not the last one
        if (config.useTempos && currentTempoIdx + 1 < score.tempos->size())
        {
            // Will loop over incoming tempo changes
            for (size_t i = currentTempoIdx + 1; i < score.tempos->size(); i++)
            {
                const TempoType &tempoChange = score.tempos->at(i);

                // if this tempo change happened before the current moment
                if (tempoChange.time <= noteToken.at(0).time)
                {
                    currentTempo = std::stod(libmusictokUtils::roundToTwoDecimals(tempoChange.qpm()));
                    currentTempoIdx++;
                }
                else if (tempoChange.time > noteToken.at(0).time)
                    break;
            }
        }

        // Positions and bars pos enc
        if (noteToken.at(0).time != currentTick)
        {
            int posIndex = (noteToken.at(0).time % ticksPerBar) / ticksPerSample;
            currentTick  = noteToken.at(0).time;
            currentPos   = posIndex;
            currentTrack = -2; // reset
            // (New bar)
            if (currentBar < currentTick / ticksPerBar)
            {
                int numNewBars = currentTick / ticksPerBar - currentBar;
                for (size_t i = 0; i < numNewBars; i++)
                {
                    std::vector<std::string> barToken = {"Bar_None", "BarPosEnc_" + std::to_string(currentBar + i + 1),
                                                         "PositionPosEnc_None"};
                    if (config.useTempos)
                    {
                        barToken.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(currentTempo));
                    }
                    tokens.push_back(barToken);
                }
                currentBar += numNewBars;
            }
            // Position
            std::vector<std::string> posToken = {"Position_" + std::to_string(currentPos),
                                                 "BarPosEnc_" + std::to_string(currentBar),
                                                 "PositionPosEnc_" + std::to_string(currentPos)};
            if (config.useTempos)
            {
                posToken.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(currentTempo));
            }
            tokens.push_back(posToken);
        }
        // Program (track)
        if (std::get<int>(noteToken.at(0).desc) != currentTrack)
        {
            currentTrack                        = std::get<int>(noteToken.at(0).desc);
            std::vector<std::string> trackToken = {"Program_" + std::to_string(currentTrack),
                                                   "BarPosEnc_" + std::to_string(currentBar),
                                                   "PositionPosEnc_" + std::to_string(currentPos)};
            if (config.useTempos)
            {
                trackToken.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(currentTempo));
            }
            tokens.push_back(trackToken);
        }

        // Adding bar and position tokens to notes for positional encoding
        std::vector<std::string> noteTokenStr;
        for (const Event &noteEvent : noteToken)
        {
            noteTokenStr.push_back(noteEvent.str());
        }
        noteTokenStr.insert(noteTokenStr.begin() + 1, "BarPosEnc_" + std::to_string(currentBar));
        noteTokenStr.insert(noteTokenStr.begin() + 2, "PositionPosEnc_" + std::to_string(currentPos));
        if (config.useTempos)
        {
            noteTokenStr.insert(noteTokenStr.begin() + 3,
                                "Tempo_" + libmusictokUtils::roundToTwoDecimals(currentTempo));
        }
        tokens.push_back(noteTokenStr);
    }

    TokSequence outTokens;
    outTokens.tokens = tokens;
    complete_sequence(outTokens);
    return outTokens;
}

//------------------------------------------------------------------------
std::vector<std::vector<Event>> MuMIDI::trackToTokens(std::shared_ptr<TrackType> track,
                                                      std::vector<std::pair<int, int>> ticksPerBeat) const
{
    std::vector<std::vector<Event>> tokens;
    int tpb = this->timeDivision;

    for (const NoteType &note : *track->notes)
    {
        int duration = note.end() - note.start();
        std::vector<Event> newToken;
        Event newEvent = Event(track->is_drum ? "PitchDrum" : "Pitch", note.pitch, note.start(), 0,
                               track->is_drum ? -1 : track->program);
        newToken.push_back(newEvent);

        if (config.useVelocities)
            newToken.emplace_back("Velocity", note.velocity);

        if (config.usingNoteDurationTokens())
        {
            std::string dur = tpbTicksToTokens.at(tpb).at(duration);
            newToken.emplace_back("Duration", dur);
        }
        tokens.push_back(newToken);
    }

    // Adds chord tokens if specified
    if (config.useChords && !track->is_drum)
    {
        std::vector<Event> chords = libmusictokUtils::detectChords(track->notes, ticksPerBeat, config.chordMaps,
                                                                   /*program=*/-1,
                                                                   /*specifyRootNote=*/true,
                                                                   /*beatRes*/ firstBeatRes,
                                                                   /*onsetOffset*/ 1,
                                                                   /*unknownChordsNumNotesRange*/ config.chordUnknown);
        std::vector<std::vector<Event>> unsqueezed;
        for (size_t c = 0; c < chords.size(); c++)
        {
            chords.at(c).desc = track->program;
            unsqueezed.push_back({chords.at(c)});
        }
        unsqueezed.insert(unsqueezed.end(), tokens.begin(), tokens.end());
        return unsqueezed;
    }
    return tokens;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> MuMIDI::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
{
    return events;
}

//------------------------------------------------------------------------
ScoreType MuMIDI::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
                                const std::vector<std::tuple<int, bool>> &programs) const
{
    TokSequence tokens = std::get<TokSequence>(tokens_);

    ScoreType score = ScoreType(this->timeDivision);

    double firstTempo;
    // Tempos
    if (config.useTempos && tokens.size() > 0)
    {
        firstTempo = std::stod(libmusictokUtils::split(tokens.tokens.getMultiVoc().at(0).at(3), "_")[1]);
    }
    else
    {
        firstTempo = this->defaultTempo;
    }
    score.tempos->append(TempoType::from_qpm(0, firstTempo));

    std::map<int, std::shared_ptr<symusic::pyvec<NoteType>>> tracks;
    int currentTick  = 0;
    int currentBar   = -1;
    int currentTrack = 0;
    int ticksPerBeat = score.ticks_per_quarter;

    for (const std::vector<std::string> &timeStep : tokens.tokens.getMultiVoc())
    {
        const auto parts           = libmusictokUtils::split(timeStep.at(0), "_");
        const std::string &tokType = parts.at(0);
        const std::string &tokVal  = parts.at(1);

        if (tokType == "Bar")
        {
            currentBar++;
            currentTick = currentBar * ticksPerBeat * 4;
        }
        else if (tokType == "Position")
        {
            if (currentBar == -1)
                currentBar = 0;
            currentTick = currentBar * ticksPerBeat * 4 + std::stoi(tokVal);
        }
        else if (tokType == "Program")
        {
            currentTrack = std::stoi(tokVal);
            if (tracks.find(currentTrack) == tracks.end())
            {
                tracks[currentTrack] = std::make_shared<symusic::pyvec<NoteType>>();
            }
        }
        else if (tokType == "Pitch" || tokType == "PitchDrum")
        {
            int velIdx         = vocabTypesIdx.at("Velocity");
            velIdx             = velIdx < 0 ? static_cast<int>(timeStep.size()) + velIdx : velIdx;
            std::string velStr = config.useVelocities ? libmusictokUtils::split(timeStep.at(velIdx), "_")[1]
                                                      : std::to_string(defaultVelocity_default);

            std::string durationStr = config.usingNoteDurationTokens()
                                          ? libmusictokUtils::split(timeStep.back(), "_")[1]
                                          : std::to_string(config.defaultNoteDuration * ticksPerBeat);

            if (durationStr == "None" || velStr == "None")
                continue;

            int pitch    = std::stoi(tokVal);
            int vel      = std::stoi(velStr);
            int duration = tpbTokensToTicks.at(ticksPerBeat).at(durationStr);
            tracks.at(currentTrack)->append(NoteType(currentTick, duration, pitch, vel));
        }
        // Decode Tempo if required
        if (config.useTempos)
        {
            double tempoVal = std::stod(libmusictokUtils::split(timeStep.at(3), "_")[1]);
            if (tempoVal != score.tempos->back().qpm())
            {
                score.tempos->append(TempoType::from_qpm(currentTick, tempoVal));
            }
        }
    }

    // Appends created notes to score object
    for (const auto &[program, notes] : tracks)
    {
        if (program == -1)
        {
            score.tracks->push_back(std::make_shared<TrackType>("Drums", 0, true));
        }
        else
        {
            score.tracks->push_back(std::make_shared<TrackType>(MIDI_INSTRUMENTS.at(program).name, program, false));
        }
        score.tracks->back()->notes = notes;
    }
    return score;
}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> MuMIDI::createBaseVocabulary()
{
    std::vector<std::vector<std::string>> vocab = {{}, {}, {}};

    // Pitch
    for (int pitch = config.pitchRange.first; pitch < config.pitchRange.second; pitch++)
    {
        vocab[0].push_back("Pitch_" + std::to_string(pitch));
    }

    // Drum Pitch
    for (int pitch = config.drumsPitchRange.first; pitch < config.drumsPitchRange.second; pitch++)
    {
        vocab[0].push_back("PitchDrum_" + std::to_string(pitch));
    }

    // Bar
    vocab[0].push_back("Bar_None");

    // Position
    int maxNumBeats = 0;
    for (const auto &ts : this->timeSignatures)
    {
        if (ts.first > maxNumBeats)
            maxNumBeats = ts.first;
    }
    int numPositions = config.maxNumPosPerBeat() * maxNumBeats;

    for (int pos = 0; pos < numPositions; pos++)
    {
        vocab[0].push_back("Position_" + std::to_string(pos));
    }

    // Programs
    for (const int &prog : config.programs)
    {
        vocab[0].push_back("Program_" + std::to_string(prog));
    }

    // Bar Pos Enc
    for (int barPos = 0; barPos < config.additionalParams["max_bar_embedding"].get<int>(); barPos++)
    {
        vocab[1].push_back("BarPosEnc_" + std::to_string(barPos));
    }

    // Position Pos Enc
    vocab[2].push_back("PositionPosEnc_None");
    for (int pos = 0; pos < numPositions; pos++)
    {
        vocab[2].push_back("PositionPosEnc_" + std::to_string(pos));
    }

    // Chord
    if (config.useChords)
    {
        std::vector<std::string> chordTokens = createChordsTokens();
        vocab[0].insert(vocab[0].end(), chordTokens.begin(), chordTokens.end());
    }

    // Rest
    if (config.useRests)
    {
        for (const std::tuple<int, int, int> &res : this->rests)
        {
            vocab[0].push_back("Rest_" + libmusictokUtils::join(res, "."));
        }
    }

    // Tempo
    if (config.useTempos)
    {
        std::vector<std::string> tempoVocabVec;
        for (const double &tempo : this->tempos)
        {
            tempoVocabVec.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(tempo));
        }
        vocab.push_back(tempoVocabVec);
    }

    // Velocity
    if (config.useVelocities)
    {
        std::vector<std::string> velocityVocabVec;
        for (const auto vel : this->velocities)
        {
            velocityVocabVec.push_back("Velocity_" + std::to_string(vel));
        }
        vocab.push_back(velocityVocabVec);
    }

    // Duration
    if (config.usingNoteDurationTokens())
    {
        std::vector<std::string> durationVocabVec;
        for (const std::tuple<int, int, int> &dur : this->durations)
        {
            durationVocabVec.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
        vocab.push_back(durationVocabVec);
    }

    return vocab;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> MuMIDI::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> dic = {
        {      "Bar",                         {"Bar", "Position"}},
        { "Position",                                 {"Program"}},
        {  "Program",                      {"Pitch", "PitchDrum"}},
        {    "Pitch",     {"Pitch", "Program", "Bar", "Position"}},
        {"PitchDrum", {"PitchDrum", "Program", "Bar", "Position"}}
    };

    if (config.useChords)
    {
        dic["Program"].insert("Chord");
        dic["Chord"] = {"Pitch"};
    }

    return dic;
}

//------------------------------------------------------------------------
int MuMIDI::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    int err                   = 0;
    std::string previousType  = libmusictokUtils::split(tokens.getMultiVoc()[0][0], "_")[0];
    int currentBar            = std::stoi(libmusictokUtils::split(tokens.getMultiVoc()[0][1], "_")[1]);
    std::string currentPosStr = libmusictokUtils::split(tokens.getMultiVoc()[0][2], "_")[1];
    int currentPos            = currentPosStr != "None" ? std::stoi(currentPosStr) : -1;
    std::vector<int> currentPitches;

    for (size_t i = 1; i < tokens.size(); i++)
    {
        const std::vector<std::string> &token = tokens.getMultiVoc().at(i);

        int barValue                        = std::stoi(libmusictokUtils::split(token[1], "_")[1]);
        std::string posValueStr             = libmusictokUtils::split(token[2], "_")[1];
        int posValue                        = posValueStr != "None" ? std::stoi(posValueStr) : -1;
        std::vector<std::string> tokenSplit = libmusictokUtils::split(token[0], "_");
        const std::string &tokenType        = tokenSplit.at(0);
        const std::string &tokenValue       = tokenSplit.at(1);

        for (const std::string &tok : token)
        {
            if (libmusictokUtils::split(tok, "_")[0] == "PAD" || libmusictokUtils::split(tok, "_")[0] == "MASK")
            {
                err++;
                continue;
            }
        }

        // Good token type
        if (tokensTypesGraph.at(previousType).find(tokenType) != tokensTypesGraph.at(previousType).end())
        {
            if (tokenType == "Bar")
            {
                currentBar++;
                currentPos = -1;
                currentPitches.clear();
            }
            else if (config.removeDuplicatedNotes && tokenType == "Pitch")
            {
                bool isError   = false;
                int tokenPitch = std::stoi(tokenValue);
                for (const int &pitch : currentPitches)
                {
                    if (pitch == tokenPitch)
                        isError = true;
                }
                if (isError)
                {
                    err++;
                }
                else
                {
                    currentPitches.push_back(tokenPitch);
                }
            }
            else if (tokenType == "Position")
            {
                int tokenPosition = std::stoi(tokenValue);

                if (tokenPosition <= currentPos || tokenPosition != posValue)
                {
                    err++;
                }
                else
                {
                    currentPos = tokenPosition;
                    currentPitches.clear();
                }
            }
            else if (tokenType == "Program")
            {
                currentPitches.clear();
            }

            if (posValue < currentPos || barValue < currentBar)
                err++;
        }
        else
        {
            err++;
        }
        previousType = tokenType;
    }
    return err;
}

//------------------------------------------------------------------------
} // namespace libmusictok
