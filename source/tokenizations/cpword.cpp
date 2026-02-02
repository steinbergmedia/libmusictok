//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/cpword.cpp
// Created by  : Steinberg, 10/2025
// Description : CP-Word tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/cpword.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
CPWord::CPWord(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kCPWord, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
CPWord::CPWord(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kCPWord, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void CPWord::tweakConfigBeforeCreatingVoc()
{
    if (config.useTimeSignatures && config.useRests)
    {
        // NOTE: this configuration could work by adding a Bar token with the new
        // TimeSig after the Rest, but the decoding should handle this to not add
        // another bar. Or it could work by making Rests not crossing new bars.
        // Rests would have a maximal value corresponding to the difference between
        // the previous event tick and the tick of the next bar. However, in cases
        // of long rests of more than one bar, we would have successions of
        // Rest --> Bar --> Rest --> Bar ... tokens.
        if (verbose)
        {
            std::clog << "You are using both Time Signatures and Rests with CPWord. Be aware "
                         "that this configuration can result in altered time, as the time "
                         "signature is carried by the Bar tokens, that are skipped during "
                         "rests. To disable this warning, you can disable either Time "
                         "Signatures or Rests. Otherwise, you can check that your data does "
                         "not have time signature changes occurring during rests."
                      << std::endl;
        }
    }

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
                         "CPWord only allows to use note duration tokens for either all "
                         "programs or none."
                      << std::endl;
        }
    }

    config.useSustainPedals  = false;
    config.usePitchBends     = false;
    config.usePitchIntervals = false;
    config.programChanges    = false;

    this->disableAttributeControls();

    std::vector<std::string> tokenTypes = {"Family", "Position", "Pitch"};

    if (config.useVelocities)
        tokenTypes.push_back("Velocity");

    if (config.usingNoteDurationTokens())
        tokenTypes.push_back("Duration");

    if (config.usePrograms)
        tokenTypes.push_back("Program");

    if (config.useChords)
        tokenTypes.push_back("Chord");

    if (config.useRests)
        tokenTypes.push_back("Rest");

    if (config.useTempos)
        tokenTypes.push_back("Tempo");

    if (config.useTimeSignatures)
        tokenTypes.push_back("TimeSig");

    size_t idx = 0;

    for (std::string tokenType : tokenTypes)
    {
        vocabTypesIdx.insert({tokenType, idx});
        ++idx;
    }

    vocabTypesIdx.insert({"Bar", 1}); // Same as "Position"
}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> CPWord::createBaseVocabulary()
{
    std::vector<std::vector<std::string>> vocab = {
        {"Family_Metric", "Family_Note"}
    };

    // Position
    // timeDivision is equal to the maximum possible ticks/beat value
    int maxNumBeats = 0;
    for (const auto &ts : this->timeSignatures)
    {
        if (ts.first > maxNumBeats)
            maxNumBeats = ts.first;
    }

    int numPositions                          = config.maxNumPosPerBeat() * maxNumBeats;
    std::vector<std::string> positionVocabVec = {"Ignore_None", "Bar_None"};
    for (size_t pos = 0; pos < numPositions; pos++)
    {
        positionVocabVec.push_back("Position_" + std::to_string(pos));
    }
    vocab.push_back(positionVocabVec);

    // Pitch
    std::vector<std::string> pitchVocabVec = {"Ignore_None"};
    for (size_t i = config.pitchRange.first; i < config.pitchRange.second; i++)
    {
        pitchVocabVec.push_back("Pitch_" + std::to_string(i));
    }
    if (config.usePitchdrumTokens)
    {
        for (size_t i = config.pitchRange.first; i < config.pitchRange.second; i++)
        {
            pitchVocabVec.push_back("PitchDrum_" + std::to_string(i));
        }
    }
    vocab.push_back(pitchVocabVec);

    // Velocity
    std::vector<std::string> velocityVocabVec = {"Ignore_None"};
    for (const auto &vel : this->velocities)
    {
        velocityVocabVec.push_back("Velocity_" + std::to_string(vel));
    }
    vocab.push_back(velocityVocabVec);

    // Duration
    if (config.usingNoteDurationTokens())
    {
        std::vector<std::string> durationVocabVec = {"Ignore_None"};
        for (const std::tuple<int, int, int> &dur : this->durations)
        {
            durationVocabVec.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
        vocab.push_back(durationVocabVec);
    }

    // Program
    if (config.usePrograms)
    {
        std::vector<std::string> programVocabVec = {"Ignore_None"};
        for (const auto &prog : config.programs)
        {
            programVocabVec.push_back("Program_" + std::to_string(prog));
        }
        vocab.push_back(programVocabVec);
    }

    // Chord
    if (config.useChords)
    {
        std::vector<std::string> chordVocabVec = {"Ignore_None"};
        std::vector<std::string> chordTokens   = createChordsTokens();
        chordVocabVec.insert(chordVocabVec.end(), chordTokens.begin(), chordTokens.end());
        vocab.push_back(chordVocabVec);
    }

    // Rest
    if (config.useRests)
    {
        std::vector<std::string> restVocabVec = {"Ignore_None"};
        for (const std::tuple<int, int, int> &res : this->rests)
        {
            restVocabVec.push_back("Rest_" + libmusictokUtils::join(res, "."));
        }
        vocab.push_back(restVocabVec);
    }

    // Tempo
    if (config.useTempos)
    {
        std::vector<std::string> tempoVocabVec = {"Ignore_None"};
        for (const double &tempo : this->tempos)
        {
            tempoVocabVec.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(tempo));
        }
        vocab.push_back(tempoVocabVec);
    }

    // Time Signature
    if (config.useTimeSignatures)
    {
        std::vector<std::string> timeSigVocabVec = {"Ignore_None"};
        for (const auto &ts : this->timeSignatures)
        {
            timeSigVocabVec.push_back("TimeSig_" + std::to_string(ts.first) + "/" + std::to_string(ts.second));
        }
        vocab.push_back(timeSigVocabVec);
    }

    return vocab;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> CPWord::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
{
    int durationOffset = 0;

    if (config.useVelocities)
        durationOffset += 1;

    if (config.usingNoteDurationTokens())
        durationOffset += 1;

    std::vector<std::vector<Event>> allEvents = {};
    int currentBar                            = -1;
    int barAtLastTsChange                     = 0;
    int previousTick                          = -1;
    int previousNoteEnd                       = 0;
    int tickAtLastTsChange                    = 0;
    int tickAtCurrentBar                      = 0;
    std::pair<int, int> currentTimeSig        = timeSignature_default;
    double currentTempo;
    if (config.logTempos)
    {
        // pick the closest to the default value
        auto minIdx  = std::min_element(tempos.begin(), tempos.end(), [this](double a, double b) {
            return std::abs(a - this->defaultTempo) < std::abs(b - this->defaultTempo);
        });
        currentTempo = *minIdx;
    }
    else
    {
        currentTempo = this->defaultTempo;
    }

    int currentProgram = -2;
    int ticksPerBar    = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
    int ticksPerBeat   = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
    int ticksPerPos    = ticksPerBeat / config.maxNumPosPerBeat();

    // First look for a TimeSig token, if any is given at tick 0, to update currentTimeSig
    if (config.useTimeSignatures)
    {
        for (const auto &event : events)
        {
            if (event.type == "TimeSig")
            {
                currentTimeSig = parseTokenTimeSignature(std::get<std::string>(event.value));
                ticksPerBar    = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);

                ticksPerBeat = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
                ticksPerPos  = ticksPerBeat / config.maxNumPosPerBeat();
                break;
            }
        }
    }

    // Then look for Tempo token, if any is given at tick 0, to update currentTempo
    if (config.useTempos)
    {
        for (const auto &event : events)
        {
            if (event.type == "Tempo")
            {
                currentTempo = std::stod(std::get<std::string>(event.value));
                break;
            }
            if (event.type == "Pitch" || event.type == "PitchDrum" || event.type == "Velocity" ||
                event.type == "Duration" || event.type == "PitchBend" || event.type == "Pedal")
            {
                break;
            }
        }
    }

    // Add the time events
    for (size_t eventIdx = 0; eventIdx < events.size(); ++eventIdx)
    {
        const Event &event = events[eventIdx];

        if (event.type == "Tempo")
            currentTempo = std::stod(std::get<std::string>(event.value));

        else if (event.type == "Program")
        {
            currentProgram = std::get<int>(event.value);
            continue;
        }

        if (event.time != previousTick)
        {
            // Rest
            if (config.useRests && event.time - previousNoteEnd > minRest(ticksPerBeat))
            {
                previousTick    = previousNoteEnd;
                auto restValues = timeTicksToTokens(event.time - previousTick, ticksPerBeat, /*rest=*/true);

                for (size_t i = 0; i < restValues.first.size(); ++i)
                {
                    const auto &durValue = restValues.first[i];
                    int durTicks         = restValues.second[i];

                    std::vector<Event> cpEvents =
                        CpTokenBuilder(previousTick, std::to_string(event.time - previousTick) + " ticks")
                            .withRest(libmusictokUtils::join(durValue, "."))
                            .build(config, vocabTypesIdx);

                    allEvents.push_back(cpEvents);
                    previousTick += durTicks;
                }

                // We update currentBar and tickAtCurrentBar here without creating Bar Tokens
                int realCurrentBar = (barAtLastTsChange + (previousTick - tickAtLastTsChange) / ticksPerBar);
                if (realCurrentBar > currentBar)
                {
                    // In case we instantly begin with a rest, we need to update currentBar
                    if (currentBar == -1)
                        currentBar = 0;

                    tickAtCurrentBar += (realCurrentBar - currentBar) * ticksPerBar;
                    currentBar = realCurrentBar;
                }
            }

            // Bar
            int numNewBars = (barAtLastTsChange + (event.time - tickAtLastTsChange) / ticksPerBar - currentBar);
            if (numNewBars >= 1)
            {
                std::string timeSigArg = "";
                if (config.useTimeSignatures)
                {
                    timeSigArg = std::to_string(currentTimeSig.first) + "/" + std::to_string(currentTimeSig.second);
                }
                for (int i = 0; i < numNewBars; ++i)
                {
                    // exception when last bar and event.type == "TimeSig"
                    if (i == numNewBars - 1 && event.type == "TimeSig")
                    {
                        auto timeSigArgTemp = parseTokenTimeSignature(std::get<std::string>(event.value));
                        timeSigArg = std::to_string(timeSigArgTemp.first) + "/" + std::to_string(timeSigArgTemp.second);
                    }

                    std::vector<Event> cpEvents = CpTokenBuilder((currentBar + i + 1) * ticksPerBar, "Bar")
                                                      .asBar()
                                                      .withTimeSignature(timeSigArg)
                                                      .build(config, vocabTypesIdx);

                    allEvents.push_back(cpEvents);
                }
                currentBar += numNewBars;
                tickAtCurrentBar = tickAtLastTsChange + (currentBar - barAtLastTsChange) * ticksPerBar;
            }

            // Position
            if (event.type != "TimeSig")
            {
                int posIndex = (event.time - tickAtCurrentBar) / ticksPerPos;

                std::vector<Event> cpEvents =
                    CpTokenBuilder(event.time, "Position")
                        .withPosition(posIndex)
                        .withChord(event.type == "Chord" ? std::get<std::string>(event.value) : "")
                        .withTempo(config.useTempos ? currentTempo : -1)
                        .build(config, vocabTypesIdx);

                allEvents.push_back(cpEvents);
            }
            previousTick = event.time;
        }

        // Update time signature time variables, after adjusting the time (above)
        if (event.type == "TimeSig")
        {
            currentTimeSig = parseTokenTimeSignature(std::get<std::string>(event.value));
            barAtLastTsChange += (event.time - tickAtLastTsChange) / ticksPerBar;

            tickAtLastTsChange = event.time;
            ticksPerBar        = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
            ticksPerBeat       = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
            ticksPerPos        = ticksPerBeat / config.maxNumPosPerBeat();

            // We decrease the previous tick so that a Position token is enforced
            // for the next event
            previousTick -= 1;
        }

        // Convert event to CP Event
        // Update max offset time of the notes encountered
        if ((event.type == "Pitch" || event.type == "PitchDrum") && eventIdx + durationOffset < events.size())
        {
            std::vector<Event> cpEvents =
                CpTokenBuilder(event.time)
                    .withPitch(std::get<int>(event.value))
                    .withVelocity(config.useVelocities ? std::get<int>(events.at(eventIdx + 1).value) : -1)
                    .withDuration(config.usingNoteDurationTokens()
                                      ? std::get<std::string>(events.at(eventIdx + durationOffset).value)
                                      : "")
                    .withProgram(currentProgram)
                    .asDrumPitch(event.type == "PitchDrum")
                    .build(config, vocabTypesIdx);

            allEvents.push_back(cpEvents);

            previousNoteEnd = std::max(previousNoteEnd, std::get<int>(event.desc));
        }
        else if (event.type == "Program" || event.type == "Tempo" || event.type == "TimeSig" || event.type == "Chord")
        {
            previousNoteEnd = std::max(previousNoteEnd, event.time);
        }
    }

    return allEvents;
}

//------------------------------------------------------------------------
ScoreType CPWord::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
                                const std::vector<std::tuple<int, bool>> &programs) const
{
    TokSequenceVec tokensTemp;
    if (config.oneTokenStreamForPrograms)
    {
        assert(std::holds_alternative<TokSequence>(tokens_));
        tokensTemp = {std::get<TokSequence>(tokens_)};
    }
    else
    {
        tokensTemp = std::get<TokSequenceVec>(tokens_);
    }

    std::vector<std::vector<std::vector<std::string>>> tokens(tokensTemp.size());

    for (size_t i = 0; i < tokensTemp.size(); ++i)
    {
        tokens[i] = tokensTemp[i].tokens.getMultiVoc();
    }

    ScoreType score = ScoreType(this->timeDivision);

    std::map<int, TrackType> tracks;
    symusic::pyvec<TempoType> tempoChanges = {TempoType::from_qpm(-1, -1)};

    symusic::pyvec<TimeSigType> timeSignatureChanges;

    int currentTick        = 0;
    int tickAtLastTsChange = 0;
    int tickAtCurrentBar   = 0;
    int currentBar         = -1;
    int barAtLastTsChange  = 0;
    int currentProgram     = 0;
    int previousNoteEnd    = 0;
    TrackType currentTrack;

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::vector<std::string>> &seq = tokens[si];

        // Handle time signatures (only for si == 0)
        if (si == 0)
        {
            if (config.useTimeSignatures)
            {
                for (const std::vector<std::string> &compoundToken : seq)
                {
                    std::string tokenFamily = libmusictokUtils::split(compoundToken[0], "_")[1];
                    if (tokenFamily == "Metric")
                    {
                        std::string barPos = libmusictokUtils::split(compoundToken[1], "_")[0];
                        if (barPos == "Bar")
                        {
                            auto [num, den] = parseTokenTimeSignature(
                                libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("TimeSig")), "_")[1]);
                            timeSignatureChanges.emplace_back(0, num, den);
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if (timeSignatureChanges.empty())
            {
                timeSignatureChanges.emplace_back(0, timeSignature_default.first, timeSignature_default.second);
            }
        }

        // Set current time signature and compute tick values
        auto currentTimeSig = timeSignatureChanges[0];
        int ticksPerBar     = libmusictokUtils::computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
        int ticksPerBeat    = tpbPerTs.at(currentTimeSig.denominator);
        int ticksPerPos     = ticksPerBeat / config.maxNumPosPerBeat();

        if (!config.oneTokenStreamForPrograms)
        {
            currentTick        = 0;
            tickAtLastTsChange = 0;
            tickAtCurrentBar   = 0;
            currentBar         = -1;
            barAtLastTsChange  = 0;
            previousNoteEnd    = 0;
            bool isDrum        = false;
            if (!programs.empty())
            {
                std::tie(currentProgram, isDrum) = programs[si];
            }
            else if (config.usePrograms)
            {
                for (const std::vector<std::string> &compoundToken : seq)
                {
                    if (libmusictokUtils::split(compoundToken[0], "_")[1] == "Note")
                    {
                        currentProgram = std::stoi(
                            libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("Program")), "_")[1]);

                        if (currentProgram == -1)
                        {
                            isDrum         = true;
                            currentProgram = 0;
                        }
                        break;
                    }
                }
            }
            currentTrack = TrackType(currentProgram == -1 ? "Drums" : MIDI_INSTRUMENTS[currentProgram].name,
                                     currentProgram, isDrum);
        }

        // Decode tokens
        for (const std::vector<std::string> &compoundToken : seq)
        {
            std::string tokenFamily = libmusictokUtils::split(compoundToken[0], "_")[1];

            if (tokenFamily == "Note")
            {
                int padRangeIdx = 3;

                if (config.useVelocities)
                    padRangeIdx++;

                if (config.usingNoteDurationTokens())
                    padRangeIdx++;

                if (config.usePrograms)
                    padRangeIdx++;

                if (std::any_of(compoundToken.begin() + 2, compoundToken.begin() + padRangeIdx,
                                [](const std::string &tok) { return libmusictokUtils::split(tok, "_")[1] == "None"; }))
                {
                    continue;
                }

                int pitch    = std::stoi(libmusictokUtils::split(compoundToken[2], "_")[1]);
                int vel      = config.useVelocities ? std::stoi(libmusictokUtils::split(compoundToken[3], "_")[1])
                                                    : defaultVelocity_default;
                int duration = 0;

                if (config.usingNoteDurationTokens())
                {
                    duration =
                        tpbTokensToTicks.at(ticksPerBeat)
                            .at(libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("Duration")), "_")[1]);
                }
                else
                {
                    duration = static_cast<int>(config.defaultNoteDuration * ticksPerBeat);
                }

                NoteType newNote(currentTick, duration, pitch, vel);

                if (config.oneTokenStreamForPrograms && config.usePrograms)
                {
                    currentProgram =
                        std::stoi(libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("Program")), "_")[1]);
                    libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                    tracks.at(currentProgram).notes->push_back(newNote);
                }
                else
                {
                    currentTrack.notes->push_back(newNote);
                }
                previousNoteEnd = std::max(previousNoteEnd, currentTick + duration);
            }
            else if (tokenFamily == "Metric")
            {
                std::string barPos = libmusictokUtils::split(compoundToken[1], "_")[0];
                if (barPos == "Bar")
                {
                    currentBar++;

                    if (currentBar > 0)
                    {
                        currentTick = tickAtCurrentBar + ticksPerBar;
                    }
                    tickAtCurrentBar = currentTick;

                    // Add new TS only if different from the last one
                    if (config.useTimeSignatures)
                    {
                        auto [num, den] = parseTokenTimeSignature(
                            libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("TimeSig")), "_")[1]);
                        if (num != currentTimeSig.numerator || den != currentTimeSig.denominator)
                        {
                            currentTimeSig = TimeSigType(currentTick, num, den);
                            if (si == 0)
                                timeSignatureChanges.push_back(currentTimeSig);

                            tickAtLastTsChange = tickAtCurrentBar;
                            barAtLastTsChange  = currentBar;
                            ticksPerBar        = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
                            ticksPerBeat       = tpbPerTs.at(currentTimeSig.denominator);
                            ticksPerPos        = ticksPerBeat / config.maxNumPosPerBeat();
                        }
                    }
                }
                else if (barPos == "Position")
                {
                    if (currentBar == -1)
                        currentBar = 0;

                    currentTick =
                        tickAtCurrentBar + std::stoi(libmusictokUtils::split(compoundToken[1], "_")[1]) * ticksPerPos;

                    if (config.useTempos && si == 0)
                    {
                        double tempo =
                            std::stod(libmusictokUtils::split(compoundToken.at(vocabTypesIdx.at("Tempo")), "_")[1]);

                        if (tempo != std::stod(libmusictokUtils::roundToTwoDecimals(tempoChanges.back().qpm())) &&
                            currentTick != tempoChanges.back().time)
                        {
                            tempoChanges.push_back(TempoType::from_qpm(currentTick, tempo));
                        }
                    }
                }
                else if (config.useRests &&
                         libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("Rest")), "_")[1] != "None")
                {
                    currentTick = std::max(previousNoteEnd, currentTick);
                    currentTick +=
                        tpbRestsToTicks.at(ticksPerBeat)
                            .at(libmusictokUtils::split(compoundToken.at(this->vocabTypesIdx.at("Rest")), "_")[1]);

                    int realCurrentBar = barAtLastTsChange + ((currentTick - tickAtLastTsChange) / ticksPerBar);
                    if (realCurrentBar > currentBar)
                    {
                        // In case we instantly begin with a rest, we need to update the currentBar
                        if (currentBar == -1)
                            currentBar = 0;

                        tickAtCurrentBar += (realCurrentBar - currentBar) * ticksPerBar;
                        currentBar = realCurrentBar;
                    }
                }
                previousNoteEnd = std::max(previousNoteEnd, currentTick);
            }
        }

        // Add currentInst to score and handle notes still active
        if (!config.oneTokenStreamForPrograms && !libmusictokUtils::Internal::isTrackEmpty(currentTrack))
        {
            score.tracks->push_back(std::make_shared<TrackType>(currentTrack));
        }
    }

    // Delete mocked
    // Ad handle first tempo (tick 0) here instead of super
    tempoChanges.erase(tempoChanges.begin());

    if (tempoChanges.empty() || (tempoChanges[0].time != 0 && std::stod(libmusictokUtils::roundToTwoDecimals(
                                                                  tempoChanges[0].qpm())) != this->defaultTempo))
    {
        tempoChanges.insert(0, TempoType::from_qpm(0, this->defaultTempo));
    }
    else if (std::stod(libmusictokUtils::roundToTwoDecimals(tempoChanges[0].qpm())) == this->defaultTempo)
    {
        tempoChanges[0].time = 0;
    }

    // Add global events to the score
    if (config.oneTokenStreamForPrograms)
    {
        for (const auto &prgTrack : tracks)
        {
            score.tracks->push_back(std::make_shared<TrackType>(prgTrack.second));
        }
    }
    score.tempos          = std::make_shared<symusic::pyvec<TempoType>>(tempoChanges);
    score.time_signatures = std::make_shared<symusic::pyvec<TimeSigType>>(timeSignatureChanges);
    return score;
}

//------------------------------------------------------------------------
std::vector<std::string> cpTokenType(std::vector<std::string> tok)
{
    std::string family = libmusictokUtils::split(tok[0], "_")[1];
    std::string msgErr = "No token type found, unknown error";
    if (family == "Note")
        return libmusictokUtils::split(tok[2], "_");

    if (family == "Metric")
    {
        std::vector<std::string> barPos = libmusictokUtils::split(tok[1], "_");
        if (barPos[0] == "Bar" || barPos[0] == "Position")
            return barPos;

        // Additional token
        for (size_t i = 1; i < 5; i++)
        {
            std::vector<std::string> decodedToken = libmusictokUtils::split(tok.at(tok.size() - i), "_");
            if (decodedToken[0] != "Ignore")
                return decodedToken;
        }
        throw std::runtime_error(msgErr);
    }

    if (family == "None")
    {
        return {"PAD", "None"};
    }
    throw std::runtime_error(msgErr);
}

//------------------------------------------------------------------------
int CPWord::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    int err                  = 0;
    std::string previousType = cpTokenType(tokens.getMultiVoc()[0])[0];
    int currentPos           = -1;
    int program              = 0;
    std::unordered_map<int, std::vector<int>> currentPitchesReset;
    for (int prog : config.programs)
    {
        currentPitchesReset[prog] = {};
    }
    std::unordered_map<int, std::vector<int>> currentPitches = currentPitchesReset;

    for (size_t i = 1; i < tokens.size(); i++)
    {
        const std::vector<std::string> &token = tokens.getMultiVoc().at(i);

        const std::vector<std::string> &parts = cpTokenType(token);
        const std::string &tokenType          = parts[0];
        const std::string &tokenValue         = parts[1];

        // Good token type
        const std::set<std::string> &tokensTypesOfPreviousType = tokensTypesGraph.at(previousType);
        if (tokensTypesOfPreviousType.find(tokenType) != tokensTypesOfPreviousType.end())
        {
            if (tokenType == "Bar")
            {
                currentPos     = -1;
                currentPitches = currentPitchesReset;
            }
            else if (config.removeDuplicatedNotes && (tokenType == "Pitch" || tokenType == "PitchDrum"))
            {
                if (config.usePrograms)
                {
                    program = std::stoi(libmusictokUtils::split(token.at(this->vocabTypesIdx.at("Program")), "_")[1]);
                }
                const std::vector<int> &curPitchForProg = currentPitches[program];
                bool isError                            = false;
                for (const int &pitch : curPitchForProg)
                {
                    if (pitch == std::stoi(tokenValue))
                    {
                        isError = true;
                    }
                }
                if (isError)
                {
                    err++;
                }
                else
                {
                    currentPitches.at(program).push_back(std::stoi(tokenValue));
                }
            }
            else if (tokenType == "Position")
            {
                if (std::stoi(tokenValue) <= currentPos && previousType != "Rest")
                {
                    err++;
                }
                else
                {
                    currentPos     = std::stoi(tokenValue);
                    currentPitches = currentPitchesReset;
                }
            }
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
std::unordered_map<std::string, std::set<std::string>> CPWord::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> dic = {
        {     "Bar",          {"Position", "Bar"}},
        {"Position",                    {"Pitch"}},
        {   "Pitch", {"Pitch", "Bar", "Position"}}
    };

    if (config.useChords)
    {
        dic["Rest"] = {"Rest", "Position"};
        dic["Pitch"].insert("Rest");
    }

    if (config.useRests)
    {
        dic["Rest"] = {"Rest", "Position", "Bar"};
        dic["Pitch"].insert("Rest");
    }

    if (config.useTempos)
    {
        // Because a tempo change can happen at an moment
        dic["Position"].insert("Position");
        dic["Position"].insert("Bar");
        if (config.useRests)
        {
            dic["Position"].insert("Rest");
            dic["Rest"].insert("Position");
        }
    }
    // Add "Ignore" to all existing keys
    for (auto &[key, values] : dic)
    {
        values.insert("Ignore");
    }

    // Create "Ignore" entry with all keys
    std::set<std::string> allKeys;
    for (const auto &[key, values] : dic)
    {
        allKeys.insert(key);
    }
    dic["Ignore"] = allKeys;

    if (config.usePitchdrumTokens)
    {
        dic["PitchDrum"] = dic["Pitch"];
        for (auto &[key, values] : dic)
        {
            if (values.count("Pitch"))
            {
                values.insert("PitchDrum");
            }
        }
    }

    return dic;
}

// CpTokenBuilder
//------------------------------------------------------------------------
std::vector<Event> CpTokenBuilder::build(const TokenizerConfig &config,
                                         const std::unordered_map<std::string, size_t> &vocabTypesIdx)
{
    auto createEvent = [this](const std::string &type, const std::variant<std::string, int> &value) -> Event {
        return Event(type, value, mTime, 0, mDesc);
    };

    std::vector<Event> cpToken = {Event("Family", "Metric", mTime, 0, mDesc), Event("Ignore", "None", mTime, 0, mDesc),
                                  Event("Ignore", "None", mTime, 0, mDesc)};

    auto checkConfigAndAdd = [&](bool configValue) {
        if (configValue)
        {
            cpToken.push_back(createEvent("Ignore", "None"));
        }
    };

    checkConfigAndAdd(config.useVelocities);
    checkConfigAndAdd(config.usingNoteDurationTokens());
    checkConfigAndAdd(config.usePrograms);
    checkConfigAndAdd(config.useChords);
    checkConfigAndAdd(config.useRests);
    checkConfigAndAdd(config.useTempos);
    checkConfigAndAdd(config.useTimeSignatures);

    if (mBar)
    {
        cpToken[1] = createEvent("Bar", "None");
        if (mTimeSig.has_value())
        {
            cpToken[vocabTypesIdx.at("TimeSig")] = createEvent("TimeSig", mTimeSig.value());
        }
    }
    else if (mPos.has_value())
    {
        cpToken[1] = createEvent("Position", mPos.value());
        if (mChord.has_value())
        {
            cpToken[vocabTypesIdx.at("Chord")] = createEvent("Chord", mChord.value());
        }
        if (mTempo.has_value())
        {
            cpToken[vocabTypesIdx.at("Tempo")] =
                createEvent("Tempo", libmusictokUtils::roundToTwoDecimals(mTempo.value()));
        }
    }
    else if (mRest.has_value())
    {
        cpToken[vocabTypesIdx.at("Rest")] = createEvent("Rest", mRest.value());
    }
    else if (mPitch.has_value())
    {
        std::string pitchTokenName = mPitchDrum ? "PitchDrum" : "Pitch";
        cpToken[0]                 = Event("Family", "Note", mTime, 0, mDesc);
        cpToken[2]                 = createEvent(pitchTokenName, mPitch.value());

        if (mVel.has_value())
        { // assuming useVelocities config check
            cpToken[3] = createEvent("Velocity", mVel.value());
        }
        if (mDur.has_value())
        {
            cpToken[vocabTypesIdx.at("Duration")] = createEvent("Duration", mDur.value());
        }
        if (mProgram.has_value())
        {
            cpToken[vocabTypesIdx.at("Program")] = createEvent("Program", mProgram.value());
        }
    }

    return cpToken;
}

//------------------------------------------------------------------------

} // namespace libmusictok
