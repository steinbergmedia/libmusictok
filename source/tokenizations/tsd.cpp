//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/tsd.cpp
// Created by  : Steinberg, 07/2025
// Description : TSD tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/tsd.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
TSD::TSD(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kTSD, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
TSD::TSD(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kTSD, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void TSD::tweakConfigBeforeCreatingVoc() {}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> TSD::createBaseVocabulary()
{
    std::vector<std::string> vocab;

    // NoteOn/NoteOff/Velocity
    addNoteTokensToVocabList(vocab);

    // TimeShift
    for (std::tuple<int, int, int> &dur : durations)
    {
        vocab.push_back("TimeShift_" + libmusictokUtils::join(dur, "."));
    }

    addAdditionalTokensToVocabList(vocab);

    return vocab;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> TSD::addTimeEvents(const std::vector<Event> &events,
                                                                                     symusic::i32 timeDivision) const
{
    std::vector<Event> allEvents;
    int previousTick    = 0;
    int previousNoteEnd = 0;
    int ticksPerBeat    = libmusictokUtils::computeTicksPerBeat(timeSignature_default.second, timeDivision);

    for (const auto &event : events)
    {
        // No time shift
        if (event.time != previousTick)
        {
            // (Rest)
            if (config.useRests && event.time - previousNoteEnd >= minRest(ticksPerBeat))
            {
                previousTick    = previousNoteEnd;
                auto restValues = timeTicksToTokens(event.time - previousTick, ticksPerBeat, /*rest=*/true);
                // Add Rest events and increment previousTick
                for (size_t i = 0; i < restValues.first.size(); ++i)
                {
                    const auto &durValue = restValues.first[i];
                    int durTicks         = restValues.second[i];
                    allEvents.emplace_back("Rest", libmusictokUtils::join(durValue, "."), previousTick, 0,
                                           std::to_string(event.time - previousTick) + " ticks");
                    previousTick += durTicks;
                }
            }

            // TimeShift
            // No else here as previous might have changed with rests
            if (event.time != previousTick)
            {
                int timeShift = event.time - previousTick;
                std::pair<std::vector<std::tuple<int, int, int>>, std::vector<int>> durValueTicks =
                    timeTicksToTokens(timeShift, ticksPerBeat);
                for (size_t i = 0; i < durValueTicks.first.size(); ++i)
                {
                    const std::tuple<int, int, int> &durValue = durValueTicks.first[i];
                    int durTicks                              = durValueTicks.second[i];
                    allEvents.emplace_back("TimeShift", libmusictokUtils::join(durValue, "."), previousTick, 0,
                                           std::to_string(timeShift) + " ticks");
                    previousTick += durTicks;
                }
            }
            previousTick = event.time;
        }
        // Update time signature time variables
        if (event.type == "TimeSig")
        {
            std::pair<int, int> currentTimeSig = parseTokenTimeSignature(std::get<std::string>(event.value));
            ticksPerBeat = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
        }

        allEvents.push_back(event);

        // Update max offset time of the notes encountered
        const std::unordered_set<std::string> pitchTypes = {"Pitch", "PitchDrum", "PitchIntervalTime",
                                                            "PitchIntervalChord"};
        const std::unordered_set<std::string> otherTypes = {"Program",  "Tempo",     "TimeSig", "Pedal",
                                                            "PedalOff", "PitchBend", "Chord"};
        if (pitchTypes.count(event.type))
        {
            // event.desc is a variant, so assume get<int>() works
            previousNoteEnd = std::max(previousNoteEnd, std::get<int>(event.desc));
        }
        else if (otherTypes.count(event.type))
        {
            previousNoteEnd = std::max(previousNoteEnd, event.time);
        }
    }

    return allEvents;
}

//------------------------------------------------------------------------
ScoreType TSD::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
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

    std::vector<std::vector<std::string>> tokens(tokensTemp.size());

    for (size_t i = 0; i < tokensTemp.size(); ++i)
    {
        tokens[i] = tokensTemp[i].tokens.get();
    }

    ScoreType score = ScoreType(this->timeDivision);
    int durOffset   = config.useVelocities ? 2 : 1;

    std::map<int, TrackType> tracks;
    symusic::pyvec<TempoType> tempoChanges;
    symusic::pyvec<TimeSigType> timeSignatureChanges;

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::string> &seq = tokens[si];

        // Tracking variables
        int currentTick     = 0;
        int currentProgram  = 0;
        int previousNoteEnd = 0;
        std::unordered_map<int, int> previousPitchOnset, previousPitchChord;
        for (int prog : config.programs)
        {
            previousPitchOnset[prog] = -128;
            previousPitchChord[prog] = -128;
        }
        std::unordered_map<int, int> activePedals;
        int ticksPerBeat = score.ticks_per_quarter;

        TrackType currentTrack;
        bool currentTrackUseDuration = false;
        // Set track program if tokens for programs are not in a single stream
        if (!config.oneTokenStreamForPrograms)
        {
            bool isDrum = false;
            if (!programs.empty())
            {
                std::tie(currentProgram, isDrum) = programs[si];
            }
            else if (config.usePrograms)
            {
                for (const std::string &token : seq)
                {
                    auto parts = libmusictokUtils::split(token, "_");
                    if (parts[0].rfind("Program", 0) == 0)
                    {
                        currentProgram = std::stoi(parts[1]);
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
        currentTrackUseDuration =
            (std::find(config.useNoteDurationPrograms.begin(), config.useNoteDurationPrograms.end(), currentProgram) !=
             config.useNoteDurationPrograms.end());

        // Decode tokens in sequence
        for (size_t ti = 0; ti < seq.size(); ++ti)
        {
            auto parts          = libmusictokUtils::split(seq[ti], "_");
            std::string tokType = parts[0];
            std::string tokVal  = parts[1];

            if (tokType == "TimeShift")
            {
                currentTick += tpbTokensToTicks.at(ticksPerBeat).at(tokVal);
            }
            else if (tokType == "Rest")
            {
                currentTick = std::max(previousNoteEnd, currentTick);
                currentTick += tpbRestsToTicks.at(ticksPerBeat).at(tokVal);
            }
            else if (tokType == "Pitch" || tokType == "PitchDrum" || tokType == "PitchIntervalTime" ||
                     tokType == "PitchIntervalChord")
            {
                int pitch = 0;
                if (tokType == "Pitch" || tokType == "PitchDrum")
                {
                    pitch = std::stoi(tokVal);
                }
                else if (tokType == "PitchIntervalTime")
                {
                    pitch = previousPitchOnset[currentProgram] + std::stoi(tokVal);
                }
                else // PitchIntervalChord
                {
                    pitch = previousPitchChord[currentProgram] + std::stoi(tokVal);
                }

                if (pitch < config.pitchRange.first || pitch > config.pitchRange.second)
                {
                    continue;
                }

                if (tokType != "PitchIntervalChord")
                {
                    previousPitchOnset[currentProgram] = pitch;
                }
                previousPitchChord[currentProgram] = pitch;

                try
                {
                    std::vector<std::string> velSplit;
                    std::string velType;
                    int vel;
                    std::vector<std::string> durSplit;
                    std::string durType;
                    int dur;

                    if (config.useVelocities)
                    {
                        velSplit = libmusictokUtils::split(seq.at(ti + 1), "_");
                        velType  = velSplit[0];
                        vel      = std::stoi(velSplit[1]);
                    }
                    else
                    {
                        velType = "Velocity";
                        vel     = defaultVelocity_default;
                    }
                    if (currentTrackUseDuration)
                    {
                        durSplit = libmusictokUtils::split(seq.at(ti + durOffset), "_");
                        durType  = durSplit[0];
                    }
                    else
                    {
                        durType = "Duration";
                        dur     = defaultNoteDuration_default * ticksPerBeat;
                    }
                    if (velType == "Velocity" && durType == "Duration")
                    {
                        if (currentTrackUseDuration)
                        {
                            dur = tpbTokensToTicks.at(ticksPerBeat).at(durSplit[1]);
                        }
                        NoteType newNote(currentTick, dur, pitch, vel);
                        if (config.oneTokenStreamForPrograms)
                        {
                            libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                            tracks[currentProgram]->notes->append(newNote);
                        }
                        else
                        {
                            currentTrack->notes->append(newNote);
                        }
                        previousNoteEnd = std::max(previousNoteEnd, currentTick + dur);
                    }
                }
                catch (const std::out_of_range &)
                {
                    // A well constituted sequence should not raise an exception
                    // However with generated sequences this can happen, or if the
                    // sequence isn't finished
                    std::clog << "bad-sequence caught and let slip in TSD::tokensToScore." << std::endl;
                }
            }
            else if (tokType == "Program")
            {
                currentProgram = std::stoi(tokVal);
                currentTrackUseDuration =
                    (std::find(config.useNoteDurationPrograms.begin(), config.useNoteDurationPrograms.end(),
                               currentProgram) != config.useNoteDurationPrograms.end());
                if (!config.oneTokenStreamForPrograms && config.programChanges)
                {
                    if (currentProgram != -1)
                    {
                        currentTrack.program = currentProgram;
                    }
                    else
                    {
                        currentTrack.program = 0;
                        currentTrack.is_drum = true;
                    }
                }
            }
            else if (tokType == "Tempo" && si == 0)
            {
                tempoChanges.emplace_back(currentTick, TempoType::qpm2mspq(std::stof(tokVal)));
            }
            else if (tokType == "TimeSig")
            {
                auto [num, den] = parseTokenTimeSignature(tokVal);

                if (si == 0)
                {
                    timeSignatureChanges.push_back(TimeSigType(currentTick, num, den));
                }
                ticksPerBeat = tpbPerTs.at(den);
            }
            else if (tokType == "Pedal")
            {
                int pedalProg = config.usePrograms ? std::stoi(tokVal) : currentProgram;
                if (config.sustainPedalDuration && ti + 1 < seq.size())
                {
                    std::vector<std::string> nextParts = libmusictokUtils::split(seq[ti + 1], "_");
                    if (nextParts[0] == "Duration")
                    {
                        int duration = tpbTokensToTicks.at(ticksPerBeat).at(nextParts[1]);
                        PedalType newPedal(currentTick, duration);
                        if (config.oneTokenStreamForPrograms)
                        {
                            libmusictokUtils::Internal::checkInst(pedalProg, tracks);
                            tracks[pedalProg].pedals->push_back(newPedal);
                        }
                        else
                        {
                            currentTrack.pedals->push_back(newPedal);
                        }
                    }
                }
                else if (activePedals.find(pedalProg) == activePedals.end())
                {
                    activePedals[pedalProg] = currentTick;
                }
            }
            else if (tokType == "PedalOff")
            {
                int pedalProg = config.usePrograms ? std::stoi(tokVal) : currentProgram;
                if (activePedals.find(pedalProg) != activePedals.end())
                {
                    PedalType newPedal(activePedals[pedalProg], currentTick - activePedals[pedalProg]);
                    if (config.oneTokenStreamForPrograms)
                    {
                        libmusictokUtils::Internal::checkInst(pedalProg, tracks);
                        tracks[pedalProg].pedals->push_back(
                            PedalType(activePedals[pedalProg], currentTick - activePedals[pedalProg]));
                    }
                    else
                    {
                        currentTrack.pedals->push_back(newPedal);
                    }
                    activePedals.erase(pedalProg);
                }
            }
            else if (tokType == "PitchBend")
            {
                PitchBendType newPitchBend(currentTick, std::stoi(tokVal));
                if (config.oneTokenStreamForPrograms)
                {
                    libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                    tracks[currentProgram].pitch_bends->push_back(newPitchBend);
                }
                else
                {
                    currentTrack.pitch_bends->push_back(newPitchBend);
                }
            }

            const std::unordered_set<std::string> otherTypes = {"Program",  "Tempo",     "TimeSig", "Pedal",
                                                                "PedalOff", "PitchBend", "Chord"};
            if (otherTypes.count(tokType))
            {
                previousNoteEnd = std::max(previousNoteEnd, currentTick);
            }
        }

        // Add current_inst to score and handle notes still active
        if (!config.oneTokenStreamForPrograms &&
            !libmusictokUtils::isTrackEmpty(currentTrack, /*checkControls*/ true, /*checkPedals*/ false,
                                            /*checkPitchBends*/ true))
        {
            score.tracks->push_back(std::make_shared<TrackType>(currentTrack));
        }
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
std::unordered_map<std::string, std::set<std::string>> TSD::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> dic;
    std::string firstNoteTokenType;

    if (config.usePrograms)
    {
        firstNoteTokenType = config.programChanges ? "Pitch" : "Program";
        dic["Program"]     = {"Pitch"};
    }
    else
    {
        firstNoteTokenType = "Pitch";
    }

    if (config.useVelocities)
    {
        dic["Pitch"]    = {"Velocity"};
        dic["Velocity"] = config.usingNoteDurationTokens() ? std::set<std::string>{"Duration"}
                                                           : std::set<std::string>{firstNoteTokenType, "TimeShift"};
    }
    else if (config.usingNoteDurationTokens())
    {
        dic["Pitch"] = {"Duration"};
    }
    else
    {
        dic["Pitch"] = {firstNoteTokenType, "TimeShift"};
    }

    if (config.usingNoteDurationTokens())
    {
        dic["Duration"] = {firstNoteTokenType, "TimeShift"};
    }
    dic["TimeShift"] = {firstNoteTokenType, "TimeShift"};

    if (config.usePitchIntervals)
    {
        for (const auto &tokenType : {"PitchIntervalTime", "PitchIntervalChord"})
        {
            if (config.useVelocities)
            {
                dic[tokenType] = {"Velocity"};
            }
            else if (config.usingNoteDurationTokens())
            {
                dic[tokenType] = {"Duration"};
            }
            else
            {
                dic[tokenType] = {firstNoteTokenType, "PitchIntervalTime", "PitchIntervalChord", "TimeShift"};
            }

            if (config.usePrograms && config.oneTokenStreamForPrograms)
            {
                dic["Program"].insert(tokenType);
            }
            else
            {
                if (config.usingNoteDurationTokens())
                {
                    dic["Duration"].insert(tokenType);
                }
                else if (config.useVelocities)
                {
                    dic["Velocity"].insert(tokenType);
                }
                else
                {
                    dic["Pitch"].insert(tokenType);
                }
                dic["TimeShift"].insert(tokenType);
            }
        }
    }

    if (config.programChanges)
    {
        auto &t = dic[(config.usingNoteDurationTokens() ? "Duration"
                                                        : (config.useVelocities ? "Velocity" : firstNoteTokenType))];
        t.insert("Program");
    }

    if (config.useChords)
    {
        dic["Chord"] = {firstNoteTokenType};
        dic["TimeShift"].insert("Chord");
        if (config.usePrograms)
        {
            dic["Program"].insert("Chord");
        }
        if (config.usePitchIntervals)
        {
            dic["Chord"].insert("PitchIntervalTime");
            dic["Chord"].insert("PitchIntervalChord");
        }
    }

    if (config.useTempos)
    {
        dic["TimeShift"].insert("Tempo");
        dic["Tempo"] = {firstNoteTokenType, "TimeShift"};
        if (config.useChords)
        {
            dic["Tempo"].insert("Chord");
        }
        if (config.useRests)
        {
            dic["Tempo"].insert("Rest");
        }
        if (config.usePitchIntervals)
        {
            dic["Tempo"].insert("PitchIntervalTime");
            dic["Tempo"].insert("PitchIntervalChord");
        }
    }

    if (config.useTimeSignatures)
    {
        dic["TimeShift"].insert("TimeSig");
        dic["TimeSig"] = {firstNoteTokenType, "TimeShift"};
        if (config.useChords)
        {
            dic["TimeSig"].insert("Chord");
        }
        if (config.useRests)
        {
            dic["TimeSig"].insert("Rest");
        }
        if (config.useTempos)
        {
            dic["TimeSig"].insert("Tempo");
        }
        if (config.usePitchIntervals)
        {
            dic["TimeSig"].insert("PitchIntervalTime");
            dic["TimeSig"].insert("PitchIntervalChord");
        }
    }

    if (config.useSustainPedals)
    {
        dic["TimeShift"].insert("Pedal");
        if (config.sustainPedalDuration)
        {
            dic["Pedal"] = {"Duration"};
            if (config.usingNoteDurationTokens())
            {
                dic["Duration"].insert("Pedal");
            }
            else if (config.useVelocities)
            {
                dic["Duration"] = {firstNoteTokenType, "TimeShift"};
                dic["Velocity"].insert("Pedal");
            }
            else
            {
                dic["Duration"] = {firstNoteTokenType, "TimeShift"};
                dic["Pitch"].insert("Pedal");
            }
        }
        else
        {
            dic["PedalOff"] = {"Pedal", "PedalOff", firstNoteTokenType, "TimeShift"};
            dic["Pedal"]    = {"Pedal", firstNoteTokenType, "TimeShift"};
            dic["TimeShift"].insert("PedalOff");
        }
        if (config.useChords)
        {
            dic["Pedal"].insert("Chord");
            if (!config.sustainPedalDuration)
            {
                dic["PedalOff"].insert("Chord");
                dic["Chord"].insert("PedalOff");
            }
        }
        if (config.useRests)
        {
            dic["Pedal"].insert("Rest");
            if (!config.sustainPedalDuration)
            {
                dic["PedalOff"].insert("Rest");
            }
        }
        if (config.useTempos)
        {
            dic["Tempo"].insert("Pedal");
            if (!config.sustainPedalDuration)
            {
                dic["Tempo"].insert("PedalOff");
            }
        }
        if (config.useTimeSignatures)
        {
            dic["TimeSig"].insert("Pedal");
            if (!config.sustainPedalDuration)
            {
                dic["TimeSig"].insert("PedalOff");
            }
        }
        if (config.usePitchIntervals)
        {
            if (config.sustainPedalDuration)
            {
                dic["Duration"].insert("PitchIntervalTime");
                dic["Duration"].insert("PitchIntervalChord");
            }
            else
            {
                dic["Pedal"].insert("PitchIntervalTime");
                dic["Pedal"].insert("PitchIntervalChord");
                dic["PedalOff"].insert("PitchIntervalTime");
                dic["PedalOff"].insert("PitchIntervalChord");
            }
        }
    }

    if (config.usePitchBends)
    {
        dic["PitchBend"] = {firstNoteTokenType, "TimeShift"};
        if (config.usePrograms && !config.programChanges)
        {
            dic["Program"].insert("PitchBend");
        }
        else
        {
            dic["TimeShift"].insert("PitchBend");
            if (config.useTempos)
            {
                dic["Tempo"].insert("PitchBend");
            }
            if (config.useTimeSignatures)
            {
                dic["TimeSig"].insert("PitchBend");
            }
            if (config.useSustainPedals)
            {
                dic["Pedal"].insert("PitchBend");
                if (config.sustainPedalDuration)
                {
                    dic["Duration"].insert("PitchBend");
                }
                else
                {
                    dic["PedalOff"].insert("PitchBend");
                }
            }
        }
        if (config.useChords)
        {
            dic["PitchBend"].insert("Chord");
        }
        if (config.useRests)
        {
            dic["PitchBend"].insert("Rest");
        }
    }

    if (config.useRests)
    {
        dic["Rest"] = {"Rest", firstNoteTokenType, "TimeShift"};
        std::string key =
            config.usingNoteDurationTokens() ? "Duration" : (config.useVelocities ? "Velocity" : firstNoteTokenType);
        dic[key].insert("Rest");
        if (config.useChords)
        {
            dic["Rest"].insert("Chord");
        }
        if (config.useTempos)
        {
            dic["Rest"].insert("Tempo");
        }
        if (config.useTimeSignatures)
        {
            dic["Rest"].insert("TimeSig");
        }
        if (config.useSustainPedals)
        {
            dic["Rest"].insert("Pedal");
            if (config.sustainPedalDuration)
            {
                dic["Duration"].insert("Rest");
            }
            else
            {
                dic["Rest"].insert("PedalOff");
                dic["PedalOff"].insert("Rest");
            }
        }
        if (config.usePitchBends)
        {
            dic["Rest"].insert("PitchBend");
        }
        if (config.usePitchIntervals)
        {
            dic["Rest"].insert("PitchIntervalTime");
            dic["Rest"].insert("PitchIntervalChord");
        }
    }
    else
    {
        dic["TimeShift"].insert("TimeShift");
    }

    if (config.programChanges)
    {
        for (const auto &tokenType :
             {"TimeShift", "Rest", "PitchBend", "Pedal", "PedalOff", "Tempo", "TimeSig", "Chord"})
        {
            if (dic.find(tokenType) != dic.end())
            {
                dic["Program"].insert(tokenType);
                dic[tokenType].insert("Program");
            }
        }
    }

    if (config.usePitchdrumTokens)
    {
        dic["PitchDrum"] = dic["Pitch"];
        for (auto &[key, values] : dic)
        {
            if (values.find("Pitch") != values.end())
            {
                values.insert("PitchDrum");
            }
        }
    }

    return dic;
}

} // namespace libmusictok
