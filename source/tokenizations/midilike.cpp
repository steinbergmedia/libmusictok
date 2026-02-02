//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/midilike.cpp
// Created by  : Steinberg, 07/2025
// Description : MIDILike tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/midilike.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
MIDILike::MIDILike(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kMIDILike, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
MIDILike::MIDILike(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kMIDILike, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void MIDILike::tweakConfigBeforeCreatingVoc()
{
    noteOnOff = true;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> MIDILike::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
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
        const std::unordered_set<std::string> pitchTypes = {"NoteOn", "DrumOn", "PitchIntervalTime",
                                                            "PitchIntervalChord"};
        const std::unordered_set<std::string> otherTypes = {"Program",  "Tempo",     "Pedal",
                                                            "PedalOff", "PitchBend", "Chord"};
        if (pitchTypes.count(event.type))
        {
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
void MIDILike::sortEvents(std::vector<Event> &events) const
{
    // MidiTok: "This could be removed if we find a way to insert NoteOff tokens before Chords"
    if (config.useChords)
    {
        std::stable_sort(events.begin(), events.end(), [this](const Event &a, const Event &b) {
            if (a.time != b.time)
            {
                return a.time < b.time;
            }
            else
            {
                return eventOrder(a) < eventOrder(b);
            }
        });

        if (attributeControls.size() > 0)
        {
            for (Event &event : events)
            {
                if (!startsWith(event.type, "ACTrack"))
                {
                    break;
                }
                event.time = 0;
            }
        }
    }
    else
    {
        MusicTokenizer::sortEvents(events);
    }
}

//------------------------------------------------------------------------
int MIDILike::eventOrder(const Event &event) const
{
    // Global tokens first
    if (event.type == "Tempo" || event.type == "TimeSig")
        return 0;
    // NoteOff or DrumOff or ProgramNoteOff
    if (event.type == "NoteOff" || event.type == "DrumOff" ||
        (event.type == "Program" && std::holds_alternative<std::string>(event.desc) &&
         std::get<std::string>(event.desc) == "ProgramNoteOff"))
        return 1;
    // Track effects (Pedal, PedalOff, PedalDuration)
    if (event.type == "Pedal" || event.type == "PedalOff" ||
        (event.type == "Duration" && std::holds_alternative<std::string>(event.desc) &&
         std::get<std::string>(event.desc) == "PedalDuration"))
        return 2;
    // PitchBend or ProgramPitchBend
    if (event.type == "PitchBend" || (event.type == "Program" && std::holds_alternative<std::string>(event.desc) &&
                                      std::get<std::string>(event.desc) == "ProgramPitchBend"))
        return 3;
    // ControlChange
    if (event.type == "ControlChange")
        return 4;
    // Track notes then
    return 10;
}

//------------------------------------------------------------------------
void MIDILike::clearActiveNotes(
    bool oneTokenStreamForPrograms, int maxDuration,
    std::unordered_map<int, std::unordered_map<int, std::vector<std::pair<int, int>>>> &activeNotes,
    std::map<int, TrackType> &tracks, TrackType *currentTrack) const
{
    if (maxDuration == -1)
        return;

    if (config.oneTokenStreamForPrograms)
    {
        for (const auto &programPair : activeNotes)
        {
            int program        = programPair.first;
            auto &activeNotes_ = programPair.second;
            for (const auto &pitchPair : activeNotes_)
            {
                int pitch     = pitchPair.first;
                auto &noteOns = pitchPair.second;
                for (const auto &[onsetTick, vel] : noteOns)
                {
                    libmusictokUtils::Internal::checkInst(program, tracks);
                    NoteType newNote(onsetTick, maxDuration, pitch, vel);
                    tracks[program].notes->push_back(newNote);
                }
            }
        }
    }
    else
    {
        auto &progNotes = activeNotes[currentTrack->program];
        for (const auto &pitchPair : progNotes)
        {
            int pitch     = pitchPair.first;
            auto &noteOns = pitchPair.second;
            for (const auto &[onsetTick, vel] : noteOns)
            {
                NoteType newNote(onsetTick, maxDuration, pitch, vel);
                currentTrack->notes->push_back(newNote);
            }
        }
    }
}

//------------------------------------------------------------------------
ScoreType MIDILike::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
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

    ScoreType score                            = ScoreType(this->timeDivision);
    std::tuple<int, int, int> maxDurationTuple = std::tuple<int, int, int>();
    bool isMaxDurationTupleLoaded              = false;
    if (config.additionalParams.find("max_duration") != config.additionalParams.end())
    {
        maxDurationTuple         = config.additionalParams.at("max_duration").get<std::tuple<int, int, int>>();
        isMaxDurationTupleLoaded = true;
    }
    int maxDuration = -1;

    std::map<int, TrackType> tracks;
    symusic::pyvec<TempoType> tempoChanges;
    symusic::pyvec<TimeSigType> timeSignatureChanges;

    // Active notes
    std::unordered_map<int, std::unordered_map<int, std::vector<std::pair<int, int>>>> activeNotes;
    for (int prog : config.programs)
    {
        std::unordered_map<int, std::vector<std::pair<int, int>>> progNotes;
        for (int pi = config.pitchRange.first; pi <= config.pitchRange.second; ++pi)
        {
            progNotes[pi] = std::vector<std::pair<int, int>>();
        }
        activeNotes[prog] = std::move(progNotes);
    }

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::string> &seq = tokens[si];

        // Tracking variables
        int currentTick    = 0;
        int currentProgram = 0;
        std::unordered_map<int, int> previousPitchOnset, previousPitchChord;
        for (int prog : config.programs)
        {
            previousPitchOnset[prog] = -128;
            previousPitchChord[prog] = -128;
        }
        std::unordered_map<int, int> activePedals;
        int ticksPerBeat = score.ticks_per_quarter;

        if (isMaxDurationTupleLoaded)
        {
            maxDuration = timeTokenToTicks(maxDurationTuple, ticksPerBeat);
        }

        TrackType currentTrack;
        bool currentTrackUsesNoteOff = false;
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
        currentTrackUsesNoteOff =
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
                currentTick += tpbRestsToTicks.at(ticksPerBeat).at(tokVal);
            }
            else if (tokType == "NoteOn" || tokType == "DrumOn" || tokType == "NoteOff" || tokType == "DrumOff" ||
                     tokType == "PitchIntervalTime" || tokType == "PitchIntervalChord")
            {
                // update previous_pitch_onset and previous_pitch_chord even if the try fails
                int pitch = 0;
                if (tokType == "PitchIntervalTime")
                {
                    pitch = previousPitchOnset[currentProgram] + std::stoi(tokVal);
                }
                else if (tokType == "PitchIntervalChord")
                {
                    pitch = previousPitchChord[currentProgram] + std::stoi(tokVal);
                }
                else
                {
                    pitch = std::stoi(tokVal);
                }

                if (pitch < config.pitchRange.first || pitch > config.pitchRange.second)
                {
                    continue;
                }

                // if NoteOn adds it to the queue with FIFO
                if (!(tokType == "NoteOff" || tokType == "DrumOff"))
                {
                    int vel;
                    if (!config.useVelocities)
                    {
                        vel = defaultVelocity_default;
                    }
                    else if (ti + 1 < seq.size())
                    {
                        try
                        {
                            vel = std::stoi(libmusictokUtils::split(seq[ti + 1], "_")[1]);
                        }
                        catch (const std::out_of_range &)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        break;
                    }

                    NoteType newNote;
                    if (!currentTrackUsesNoteOff)
                    {
                        newNote = NoteType(currentTick, static_cast<int>(defaultNoteDuration_default * ticksPerBeat),
                                           pitch, vel);
                        if (config.oneTokenStreamForPrograms)
                        {
                            libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                            tracks[currentProgram].notes->push_back(newNote);
                        }
                        else
                        {
                            currentTrack.notes->push_back(newNote);
                        }
                    }
                    else
                    {
                        activeNotes[currentProgram][pitch].push_back({currentTick, vel});
                    }
                    previousPitchChord[currentProgram] = pitch;

                    if (tokType != "PitchIntervalChord")
                    {
                        previousPitchOnset[currentProgram] = pitch;
                    }
                }
                // NoteOff/DrumOff, creates the note
                else if ((activeNotes[currentProgram][pitch]).size() > 0)
                {
                    auto [noteOnsetTick, vel] = activeNotes[currentProgram][pitch].front();
                    activeNotes[currentProgram][pitch].erase(activeNotes[currentProgram][pitch].begin());
                    int duration = currentTick - noteOnsetTick;
                    if (maxDuration != -1 && duration > maxDuration)
                    {
                        duration = maxDuration;
                    }
                    NoteType newNote(noteOnsetTick, duration, pitch, vel);
                    if (config.oneTokenStreamForPrograms)
                    {
                        libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                        tracks[currentProgram].notes->push_back(newNote);
                    }
                    else
                    {
                        currentTrack.notes->push_back(newNote);
                    }
                }
            }
            else if (tokType == "Program")
            {
                currentProgram = std::stoi(tokVal);
                currentTrackUsesNoteOff =
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
                ticksPerBeat    = tpbPerTs.at(den);
                if (maxDuration != -1)
                {
                    maxDuration = timeTokenToTicks(maxDurationTuple, ticksPerBeat);
                }
                if (si == 0)
                {
                    timeSignatureChanges.push_back(TimeSigType(currentTick, num, den));
                }
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
        }

        // Add current_inst to score and handle notes still active
        if (!config.oneTokenStreamForPrograms &&
            !libmusictokUtils::isTrackEmpty(currentTrack, /*checkControls*/ true, /*checkPedals*/ false,
                                            /*checkPitchBends*/ true))
        {
            score.tracks->push_back(std::make_shared<TrackType>(currentTrack));
            clearActiveNotes(config.oneTokenStreamForPrograms, maxDuration, activeNotes, tracks, &currentTrack);
            // Reset activeNotes[currentTrack.program]
            std::unordered_map<int, std::vector<std::pair<int, int>>> progNotes;
            for (int pi = config.pitchRange.first; pi <= config.pitchRange.second; ++pi)
            {
                progNotes[pi] = std::vector<std::pair<int, int>>();
            }
            activeNotes[currentTrack.program] = std::move(progNotes);
        }
    }

    // Handle notes still active (for single-stream/token-per-programs case)
    if (config.oneTokenStreamForPrograms)
    {
        clearActiveNotes(config.oneTokenStreamForPrograms, maxDuration, activeNotes, tracks, nullptr);
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
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> MIDILike::createBaseVocabulary()
{
    std::vector<std::string> vocab;

    // NoteOn/NoteOff/Velocity
    addNoteTokensToVocabList(vocab);

    // TimeShift
    for (const std::tuple<int, int, int> &dur : durations)
    {
        vocab.push_back("TimeShift_" + libmusictokUtils::join(dur, "."));
    }

    addAdditionalTokensToVocabList(vocab);

    // Add durations if needed
    if (config.useSustainPedals && config.sustainPedalDuration)
    {
        for (const std::tuple<int, int, int> &dur : durations)
        {
            vocab.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
    }

    return vocab;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> MIDILike::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> dic;
    std::string firstNoteTokenType;
    std::string lastNoteTokenType;

    if (config.usePrograms)
    {
        firstNoteTokenType = config.programChanges ? "NoteOn" : "Program";
        dic["Program"]     = {"NoteOn"};
        if (config.usingNoteDurationTokens())
        {
            dic["Program"].insert("NoteOff");
        }
    }
    else
    {
        firstNoteTokenType = "NoteOn";
    }

    if (config.useVelocities)
    {
        dic["NoteOn"]     = {"Velocity"};
        dic["Velocity"]   = {firstNoteTokenType, "TimeShift"};
        lastNoteTokenType = "Velocity";
    }
    else
    {
        dic["NoteOn"]     = {firstNoteTokenType, "TimeShift"};
        lastNoteTokenType = "NoteOn";
    }

    dic["TimeShift"] = {firstNoteTokenType, "TimeShift"};

    if (config.usingNoteDurationTokens())
    {
        dic["NoteOff"] = {"NoteOff", firstNoteTokenType, "TimeShift"};
        dic["TimeShift"].insert("NoteOff");
    }

    if (config.usePitchIntervals)
    {
        for (const auto &tokenType : {"PitchIntervalTime", "PitchIntervalChord"})
        {
            dic[tokenType] = {lastNoteTokenType};
            if (!config.useVelocities)
            {
                dic[tokenType].insert("PitchIntervalTime");
                dic[tokenType].insert("PitchIntervalChord");
                dic[tokenType].insert("TimeShift");
            }
            if (config.usePrograms && config.oneTokenStreamForPrograms)
            {
                dic["Program"].insert(tokenType);
            }
            else
            {
                dic[lastNoteTokenType].insert(tokenType);
                if (config.usingNoteDurationTokens())
                    dic["NoteOff"].insert(tokenType);
                else if (config.useVelocities)
                    dic["Velocity"].insert(tokenType);
                else
                    dic["NoteOn"].insert(tokenType);
                dic["TimeShift"].insert(tokenType);
            }
        }
    }

    if (config.programChanges)
    {
        dic[lastNoteTokenType].insert("Program");
        if (config.usingNoteDurationTokens())
            dic["NoteOff"].insert("Program");
    }

    if (config.useChords)
    {
        dic["Chord"] = {firstNoteTokenType};
        dic["TimeShift"].insert("Chord");
        if (config.usingNoteDurationTokens())
        {
            dic["NoteOff"].insert("Chord");
        }
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
        if (config.usingNoteDurationTokens() && (!config.usePrograms || config.programChanges))
        {
            dic["Tempo"].insert("NoteOff");
        }
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
        if (config.usingNoteDurationTokens() && (!config.usePrograms || config.programChanges))
        {
            dic["TimeSig"].insert("NoteOff");
        }
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
        if (config.usingNoteDurationTokens())
        {
            dic["NoteOff"].insert("Pedal");
            if (!config.sustainPedalDuration)
            {
                dic["NoteOff"].insert("PedalOff");
            }
        }

        if (config.sustainPedalDuration)
        {
            dic["Pedal"]    = {"Duration"};
            dic["Duration"] = {firstNoteTokenType, "TimeShift", "Pedal"};
            if (config.usingNoteDurationTokens())
            {
                dic["Duration"].insert("NoteOff");
            }

            if (config.usePitchIntervals)
            {
                dic["Duration"].insert("PitchIntervalTime");
                dic["Duration"].insert("PitchIntervalChord");
            }
        }
        else
        {
            dic["PedalOff"] = {"Pedal", "PedalOff", firstNoteTokenType, "TimeShift"};
            dic["Pedal"]    = {"Pedal", firstNoteTokenType, "TimeShift"};

            if (config.usingNoteDurationTokens())
            {
                dic["Pedal"].insert("NoteOff");
                dic["PedalOff"].insert("NoteOff");
            }

            dic["TimeShift"].insert("PedalOff");

            if (config.usePitchIntervals)
            {
                dic["Pedal"].insert("PitchIntervalTime");
                dic["Pedal"].insert("PitchIntervalChord");
                dic["PedalOff"].insert("PitchIntervalTime");
                dic["PedalOff"].insert("PitchIntervalChord");
            }
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
    }

    if (config.usePitchBends)
    {
        dic["PitchBend"] = {firstNoteTokenType, "TimeShift"};
        if (config.usingNoteDurationTokens())
        {
            dic["PitchBend"].insert("NoteOff");
        }
        if (config.usePrograms && !config.programChanges)
        {
            dic["Program"].insert("PitchBend");
        }
        else
        {
            dic["TimeShift"].insert("PitchBend");
            if (config.usingNoteDurationTokens())
            {
                dic["NoteOff"].insert("PitchBend");
            }
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
        if (config.usingNoteDurationTokens())
        {
            dic["NoteOff"].insert("Rest");
        }
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
        std::vector<std::pair<std::string, std::string>> tokList = {
            {"DrumOn", "NoteOn"}
        };
        if (config.usingNoteDurationTokens())
        {
            tokList.push_back({"DrumOff", "NoteOff"});
        }
        for (auto &[tok1, tok2] : tokList)
        {
            dic[tok1] = dic[tok2];
            for (auto &[key, values] : dic)
            {
                if (values.find(tok2) != values.end())
                {
                    values.insert(tok1);
                }
            }
        }
    }

    return dic;
}

//------------------------------------------------------------------------
int MIDILike::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    int err            = 0;
    int currentProgram = 0;
    int currentTick    = 0;
    std::unordered_map<int, std::unordered_map<int, std::vector<int>>> activePitches;
    std::unordered_map<int, std::vector<int>> currentPitchesTick;
    std::unordered_map<int, int> previousPitchOnset;
    std::unordered_map<int, int> previousPitchChord;

    for (int prog : config.programs)
    {
        activePitches[prog] = {};
        for (int pitch = config.pitchRange.first; pitch < config.pitchRange.second + 1; ++pitch)
        {
            activePitches[prog][pitch] = {};
        }
        currentPitchesTick[prog] = {};
        previousPitchOnset[prog] = -128;
        previousPitchChord[prog] = -128;
    }

    int ticksPerBeat = timeDivision;

    int maxDuration                            = -1;
    std::tuple<int, int, int> maxDurationTuple = std::tuple<int, int, int>();
    if (config.additionalParams.find("max_duration") != config.additionalParams.end())
    {
        maxDurationTuple = config.additionalParams.at("max_duration").get<std::tuple<int, int, int>>();
        maxDuration      = timeTokenToTicks(maxDurationTuple, ticksPerBeat);
    }

    std::vector<Event> events;
    for (const std::string &token : tokens)
    {
        std::vector<std::string> parts = libmusictokUtils::split(token, "_");
        events.emplace_back(parts[0], parts[1]);
    }

    for (size_t i = 0; i < events.size(); ++i)
    {
        // Bad token type
        if (i > 0)
        {
            const auto &prevType = events[i - 1].type;
            const auto &currType = events[i].type;
            if (tokensTypesGraph.at(prevType).find(currType) == tokensTypesGraph.at(prevType).end())
            {
                ++err;
            }
        }
        // Good token type
        else if (events[i].type == "NoteOn" || events[i].type == "DrumOn" || events[i].type == "PitchIntervalTime" ||
                 events[i].type == "PitchIntervalChord")
        {
            int pitchVal;
            if (events[i].type == "NoteOn" || events[i].type == "DrumOn")
            {
                pitchVal                           = std::holds_alternative<int>(events[i].value)
                                                         ? std::get<int>(events[i].value)
                                                         : std::stoi(std::get<std::string>(events[i].value));
                previousPitchOnset[currentProgram] = pitchVal;
                previousPitchChord[currentProgram] = pitchVal;
            }
            else if (events[i].type == "PitchIntervalTime")
            {
                pitchVal =
                    previousPitchOnset[currentProgram] + (std::holds_alternative<int>(events[i].value)
                                                              ? std::get<int>(events[i].value)
                                                              : std::stoi(std::get<std::string>(events[i].value)));
                previousPitchOnset[currentProgram] = pitchVal;
                previousPitchChord[currentProgram] = pitchVal;
            }
            else // PitchIntervalChord
            {
                pitchVal =
                    previousPitchChord[currentProgram] + (std::holds_alternative<int>(events[i].value)
                                                              ? std::get<int>(events[i].value)
                                                              : std::stoi(std::get<std::string>(events[i].value)));
                previousPitchChord[currentProgram] = pitchVal;
            }

            if (config.usingNoteDurationTokens())
                activePitches[currentProgram][pitchVal].push_back(currentTick);

            if (config.removeDuplicatedNotes &&
                std::find(currentPitchesTick[currentProgram].begin(), currentPitchesTick[currentProgram].end(),
                          pitchVal) != currentPitchesTick[currentProgram].end())
            {
                ++err; // note already being played at current tick
                continue;
            }
            currentPitchesTick[currentProgram].push_back(pitchVal);
        }
        else if (events[i].type == "NoteOff" || events[i].type == "DrumOff")
        {
            if (activePitches[currentProgram][std::holds_alternative<int>(events[i].value)
                                                  ? std::get<int>(events[i].value)
                                                  : std::stoi(std::get<std::string>(events[i].value))]
                    .size() == 0)
            {
                ++err; // this pitch wasn't being played
                continue;
            }
            // Check if duration is not exceeding limit
            int noteOnsetTick = activePitches[currentProgram][std::holds_alternative<int>(events[i].value)
                                                                  ? std::get<int>(events[i].value)
                                                                  : std::stoi(std::get<std::string>(events[i].value))]
                                    .front();
            activePitches[currentProgram][std::holds_alternative<int>(events[i].value)
                                              ? std::get<int>(events[i].value)
                                              : std::stoi(std::get<std::string>(events[i].value))]
                .erase(activePitches[currentProgram][std::holds_alternative<int>(events[i].value)
                                                         ? std::get<int>(events[i].value)
                                                         : std::stoi(std::get<std::string>(events[i].value))]
                           .begin());
            int duration = currentTick - noteOnsetTick;
            if (maxDuration != -1 && duration > maxDuration)
            {
                ++err;
            }
        }
        else if (events[i].type == "Program" && i + 1 < events.size())
        {
            currentProgram = std::holds_alternative<int>(events[i].value)
                                 ? std::get<int>(events[i].value)
                                 : std::stoi(std::get<std::string>(events[i].value));
        }
        else if (events[i].type == "TimeShift" || events[i].type == "Rest")
        {
            for (int p : config.programs)
            {
                currentPitchesTick[p] = {};
            }
            if (events[i].type == "TimeShift")
            {
                currentTick += tpbTokensToTicks.at(ticksPerBeat)
                                   .at(std::holds_alternative<std::string>(events[i].value)
                                           ? std::get<std::string>(events[i].value)
                                           : std::to_string(std::get<int>(events[i].value)));
            }
            else
            {
                currentTick += tpbRestsToTicks.at(ticksPerBeat)
                                   .at(std::holds_alternative<std::string>(events[i].value)
                                           ? std::get<std::string>(events[i].value)
                                           : std::to_string(std::get<int>(events[i].value)));
            }
        }
        else if (events[i].type == "TimeSig")
        {
            auto [num, den] = parseTokenTimeSignature(std::get<std::string>(events[i].value));
            ticksPerBeat    = tpbPerTs.at(den);
            if (maxDuration != -1)
            {
                maxDuration = timeTokenToTicks(maxDurationTuple, ticksPerBeat);
            }
        }
    }
    // Check for un-ended notes
    for (auto &kv : activePitches)
    {
        for (auto &keyVal : kv.second)
        {
            err += keyVal.second.size();
        }
    }

    return err;
}

//------------------------------------------------------------------------

} // namespace libmusictok
