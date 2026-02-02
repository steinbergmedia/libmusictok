//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/octuple.cpp
// Created by  : Steinberg, 10/2025
// Description : Octuple tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/octuple.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
Octuple::Octuple(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kOctuple, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
Octuple::Octuple(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kOctuple, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void Octuple::tweakConfigBeforeCreatingVoc()
{
    config.useRests                          = false;
    config.useRests                          = false;
    config.useSustainPedals                  = false;
    config.usePitchBends                     = false;
    config.usePitchIntervals                 = false;
    config.deleteEqualSuccessiveTempoChanges = true;
    config.programChanges                    = false;

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
                         "Octuple only allows to use note duration tokens for either all "
                         "programs or none."
                      << std::endl;
        }
    }

    if (config.additionalParams.find("max_bar_embedding") == config.additionalParams.end())
    {
        config.additionalParams["max_bar_embedding"] = 60;
    }

    std::vector<std::string> tokenTypes = {"Pitch", "Position", "Bar"};

    if (config.useVelocities)
        tokenTypes.push_back("Velocity");

    if (config.usingNoteDurationTokens())
        tokenTypes.push_back("Duration");

    if (config.usePrograms)
        tokenTypes.push_back("Program");

    if (config.useTempos)
        tokenTypes.push_back("Tempo");

    if (config.useTimeSignatures)
        tokenTypes.push_back("TimeSig");

    for (int i = 0; i < tokenTypes.size(); i++)
    {
        vocabTypesIdx[tokenTypes.at(i)] = i;
    }
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> Octuple::scoreToTokens(const ScoreType &score_,
                                                                           const AttributeControlsIndexesType &_) const
{
    ScoreType score           = score_;
    std::vector<int> barTicks = libmusictokUtils::getBarsTicks(score, true);
    int maxBarEmbedding       = config.additionalParams.at("max_bar_embedding").get<int>();
    if (maxBarEmbedding < barTicks.size())
    {
        score.clip_inplace(0, barTicks.at(maxBarEmbedding));
        if (verbose)
        {
            std::clog << "Octuple cannot entirely tokenize this file "
                         "as it contains "
                      << std::to_string(barTicks.size()) << "bars whereas the limit of the tokenizer is "
                      << std::to_string(maxBarEmbedding) << ". It is therefore clipped to "
                      << std::to_string(maxBarEmbedding) << " bars." << std::endl;
        }
    }

    return MusicTokenizer::scoreToTokens(score);
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> Octuple::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
{
    int durationOffset = 0;

    if (config.useVelocities)
        durationOffset++;

    if (config.usingNoteDurationTokens())
        durationOffset++;

    std::vector<std::vector<Event>> allEvents;
    int currentBar                     = 0;
    int currentBarFromTsTime           = 0;
    int currentTickFromTsTime          = 0;
    int currentPos                     = 0;
    int previousTick                   = 0;
    std::pair<int, int> currentTimeSig = timeSignature_default;
    std::string currentTempo           = libmusictokUtils::roundToTwoDecimals(this->defaultTempo);
    int currentProgram                 = 0;
    int ticksPerBar                    = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
    int ticksPerBeat                   = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
    int ticksPerPos                    = ticksPerBeat / config.maxNumPosPerBeat();

    for (size_t e = 0; e < events.size(); e++)
    {
        const Event &event = events.at(e);

        // Set current bar and position
        // This is done first as we need to compute these values with the current
        // ticksPerBar, which might change if the current event is a TimeSig
        if (event.time != previousTick)
        {
            int elapsedTick      = event.time - currentTickFromTsTime;
            currentBar           = currentBarFromTsTime + elapsedTick / ticksPerBar;
            int tickAtCurrentBar = currentTickFromTsTime + (currentBar - currentBarFromTsTime) * ticksPerBar;
            currentPos           = (event.time - tickAtCurrentBar) / ticksPerPos;
            previousTick         = event.time;
        }

        if (event.type == "TimeSig")
        {
            currentTimeSig        = parseTokenTimeSignature(std::get<std::string>(event.value));
            currentBarFromTsTime  = currentBar;
            currentTickFromTsTime = previousTick;
            ticksPerBar           = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
            ticksPerBeat          = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
        }

        else if (event.type == "Tempo")
            currentTempo = std::get<std::string>(event.value);

        else if (event.type == "Program")
            currentProgram = std::get<int>(event.value);

        else if ((event.type == "Pitch" || event.type == "PitchDrum") && e + durationOffset < events.size())
        {
            const std::string &pitchTokenName = event.type;
            std::vector<Event> newEvent       = {Event(pitchTokenName, event.value, /*time*/ event.time),
                                                 Event("Position", currentPos, /*time*/ event.time),
                                                 Event("Bar", currentBar, /*time*/ event.time)};

            if (config.useVelocities)
                newEvent.emplace_back("Velocity", events.at(e + 1).value, /*time*/ event.time);

            if (config.usingNoteDurationTokens())
                newEvent.emplace_back("Duration", events.at(e + durationOffset).value, /*time*/ event.time);

            if (config.usePrograms)
                newEvent.emplace_back("Program", currentProgram);

            if (config.useTempos)
                newEvent.emplace_back("Tempo", currentTempo);

            if (config.useTimeSignatures)
                newEvent.emplace_back("TimeSig", std::to_string(currentTimeSig.first) + "/" +
                                                     std::to_string(currentTimeSig.second));

            allEvents.push_back(newEvent);
        }
    }
    return allEvents;
}

//------------------------------------------------------------------------
ScoreType Octuple::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
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

    int barAtLastTsChange  = 0;
    int tickAtLastTsChange = 0;
    int currentProgram     = 0;
    int ticksPerBar        = 0;
    int ticksPerBeat       = 0;
    int ticksPerPos        = 0;

    TrackType currentTrack;
    TimeSigType currentTimeSig;

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::vector<std::string>> &seq = tokens.at(si);

        // first look for the first time signature if needed
        if (si == 0 && config.useTimeSignatures)
        {
            const auto &[num, den] =
                parseTokenTimeSignature(libmusictokUtils::split(seq.at(0).at(vocabTypesIdx.at("TimeSig")), "_")[1]);
            timeSignatureChanges.push_back(TimeSigType(0, num, den));
        }
        else
        {
            timeSignatureChanges.push_back(TimeSigType(0, timeSignature_default.first, timeSignature_default.second));
        }
        TimeSigType currentTimeSig = timeSignatureChanges.at(0);
        ticksPerBar                = libmusictokUtils::computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
        ticksPerBeat               = tpbPerTs.at(currentTimeSig.denominator);
        ticksPerPos                = ticksPerBeat / config.maxNumPosPerBeat();

        // Set track / sequence program if needed
        if (!config.oneTokenStreamForPrograms)
        {
            bool isDrum = false;
            if (!programs.empty())
            {
                std::tie(currentProgram, isDrum) = programs.at(si);
            }
            else if (config.usePrograms && seq.size() > 0)
            {
                currentProgram = std::stoi(libmusictokUtils::split(seq.at(0).at(vocabTypesIdx.at("Program")), "_")[1]);
                if (currentProgram == -1)
                {
                    isDrum         = true;
                    currentProgram = 0;
                }
            }
            currentTrack = TrackType(currentProgram == -1 ? "Drums" : MIDI_INSTRUMENTS[currentProgram].name,
                                     currentProgram, isDrum);
        }

        // Decode tokens
        for (const std::vector<std::string> &timeStep : seq)
        {
            int numTokToCheck = 3;

            if (config.useVelocities)
                numTokToCheck++;

            if (config.usingNoteDurationTokens())
                numTokToCheck++;

            if (config.usePrograms)
            {
                currentProgram = std::stoi(libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("Program")), "_")[1]);
                numTokToCheck++;
            }

            if (std::any_of(timeStep.begin() + 2, timeStep.begin() + numTokToCheck,
                            [](const std::string &tok) { return libmusictokUtils::split(tok, "_")[1] == "None"; }))
            {
                continue;
            }

            // Note Attributes
            int pitch = std::stoi(libmusictokUtils::split(timeStep[0], "_")[1]);
            int vel   = config.useVelocities
                            ? std::stoi(libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("Velocity")), "_")[1])
                            : defaultVelocity_default;

            // Time Values
            int eventPos = std::stoi(libmusictokUtils::split(timeStep[1], "_")[1]);
            int eventBar = std::stoi(libmusictokUtils::split(timeStep[2], "_")[1]);
            int currentTick =
                tickAtLastTsChange + (eventBar - barAtLastTsChange) * ticksPerBar + eventPos * ticksPerPos;

            // Time Signature, adds a TimeSignatureChange if necessary
            if (config.useTimeSignatures &&
                libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("TimeSig")), "_")[1] != "None")
            {
                const auto &[num, den] =
                    parseTokenTimeSignature(libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("TimeSig")), "_")[1]);

                if (num != currentTimeSig.numerator || den != currentTimeSig.denominator)
                {
                    tickAtLastTsChange += (eventBar - barAtLastTsChange) * ticksPerBar;
                    currentTimeSig = TimeSigType(tickAtLastTsChange, num, den);

                    if (si == 0)
                        timeSignatureChanges.push_back(currentTimeSig);

                    barAtLastTsChange = eventBar;
                    ticksPerBar       = libmusictokUtils::computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
                    ticksPerBeat      = tpbPerTs.at(currentTimeSig.denominator);
                    ticksPerPos       = ticksPerBeat / config.maxNumPosPerBeat();
                }
            }
            // Note Duration
            int duration;
            if (config.usingNoteDurationTokens())
            {
                duration = tpbTokensToTicks.at(ticksPerBeat)
                               .at(libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("Duration")), "_")[1]);
            }
            else
            {
                duration = config.defaultNoteDuration * ticksPerBeat;
            }

            // Append the created note
            auto newNote = NoteType(currentTick, duration, pitch, vel);
            if (config.oneTokenStreamForPrograms)
            {
                libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                tracks.at(currentProgram).notes->push_back(newNote);
            }
            else
            {
                currentTrack.notes->push_back(newNote);
            }

            // Tempo, adds a TempoChange if necessary
            if (si == 0 && config.useTempos &&
                libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("Tempo")), "_")[1] != "None")
            {
                double tempo = std::stod(libmusictokUtils::split(timeStep.at(vocabTypesIdx.at("Tempo")), "_")[1]);

                if (tempo != std::stod(libmusictokUtils::roundToTwoDecimals(tempoChanges.back().qpm())))
                {
                    tempoChanges.push_back(TempoType::from_qpm(currentTick, tempo));
                }
            }
        }
        // Add current track to score
        if (!config.oneTokenStreamForPrograms && !libmusictokUtils::isTrackEmpty(currentTrack))
        {
            score.tracks->push_back(std::make_shared<TrackType>(currentTrack));
        }
    }

    // delete mocked
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
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> Octuple::createBaseVocabulary()
{
    std::vector<std::vector<std::string>> vocab;

    // Pitch
    std::vector<std::string> pitchVocab;
    for (size_t p = config.pitchRange.first; p < config.pitchRange.second; p++)
    {
        pitchVocab.push_back("Pitch_" + std::to_string(p));
    }
    vocab.push_back(pitchVocab);

    // PitchDrum
    if (config.usePitchdrumTokens)
    {
        for (size_t p = config.drumsPitchRange.first; p < config.drumsPitchRange.second; p++)
        {
            vocab.at(0).push_back("PitchDrum_" + std::to_string(p));
        }
    }

    // Position
    int maxNumBeats = 0;
    for (const auto &ts : this->timeSignatures)
    {
        if (ts.first > maxNumBeats)
            maxNumBeats = ts.first;
    }

    int numPositions = config.maxNumPosPerBeat() * maxNumBeats;
    std::vector<std::string> positionVocab;
    for (size_t pos = 0; pos < numPositions; pos++)
    {
        positionVocab.push_back("Position_" + std::to_string(pos));
    }
    vocab.push_back(positionVocab);

    // Bar (positional encoding)
    std::vector<std::string> barVocab;
    for (int barPos = 0; barPos < config.additionalParams["max_bar_embedding"].get<int>(); barPos++)
    {
        barVocab.push_back("Bar_" + std::to_string(barPos));
    }
    vocab.push_back(barVocab);

    // Optional Tokens

    // Velocity
    if (config.useVelocities)
    {
        std::vector<std::string> velVocab;
        for (const auto &vel : velocities)
        {
            velVocab.push_back("Velocity_" + std::to_string(vel));
        }
        vocab.push_back(velVocab);
    }

    // Duration
    if (config.usingNoteDurationTokens())
    {
        std::vector<std::string> durVocab;
        for (const std::tuple<int, int, int> &dur : this->durations)
        {
            durVocab.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
        vocab.push_back(durVocab);
    }

    // Program
    if (config.usePrograms)
    {
        std::vector<std::string> progVocab;
        for (const auto &prog : config.programs)
        {
            progVocab.push_back("Program_" + std::to_string(prog));
        }
        vocab.push_back(progVocab);
    }

    // Tempo
    if (config.useTempos)
    {
        std::vector<std::string> tempoVocab;
        for (const double &tempo : this->tempos)
        {
            tempoVocab.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(tempo));
        }
        vocab.push_back(tempoVocab);
    }

    // Time Signature
    if (config.useTimeSignatures)
    {
        std::vector<std::string> timeSigVocab;
        for (const auto &ts : this->timeSignatures)
        {
            timeSigVocab.push_back("TimeSig_" + std::to_string(ts.first) + "/" + std::to_string(ts.second));
        }
        vocab.push_back(timeSigVocab);
    }

    return vocab;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> Octuple::createTokensTypesGraph()
{
    return {};
}

//------------------------------------------------------------------------
int Octuple::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    int err            = 0;
    int currentBar     = -1;
    int currentPos     = -1;
    int currentProgram = 0;

    std::unordered_map<int, std::vector<int>> currentPitchesReset;
    for (int prog : config.programs)
    {
        currentPitchesReset[prog] = {};
    }
    std::unordered_map<int, std::vector<int>> currentPitches = currentPitchesReset;

    for (const std::vector<std::string> &token : tokens.getMultiVoc())
    {
        if (std::any_of(token.begin(), token.end(),
                        [](const std::string &tok) { return libmusictokUtils::split(tok, "_")[1] == "None"; }))
        {
            err++;
            continue;
        }

        bool hasError  = false;
        int barValue   = std::stoi(libmusictokUtils::split(token[2], "_")[1]);
        int posValue   = std::stoi(libmusictokUtils::split(token[1], "_")[1]);
        int pitchValue = std::stoi(libmusictokUtils::split(token[0], "_")[1]);

        if (config.usePrograms)
        {
            currentProgram = std::stoi(libmusictokUtils::split(token.at(vocabTypesIdx.at("Program")), "_")[1]);
        }

        // Bar
        if (barValue < currentBar)
            hasError = true;
        else if (barValue > currentBar)
        {
            currentBar     = barValue;
            currentPos     = -1;
            currentPitches = currentPitchesReset;
        }

        // Position
        if (posValue < currentPos)
            hasError = true;
        else if (posValue > currentPos)
        {
            currentPos     = posValue;
            currentPitches = currentPitchesReset;
        }

        // Pitch
        if (config.removeDuplicatedNotes)
        {
            if (std::find(currentPitches.at(currentProgram).begin(), currentPitches.at(currentProgram).end(),
                          pitchValue) != currentPitches.at(currentProgram).end())
            {
                hasError = true;
            }
            else
            {
                currentPitches.at(currentProgram).push_back(pitchValue);
            }
        }

        if (hasError)
            err++;
    }
    return err;
}

//------------------------------------------------------------------------
} // namespace libmusictok
