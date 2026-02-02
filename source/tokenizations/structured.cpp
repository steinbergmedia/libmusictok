//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/structured.cpp
// Created by  : Steinberg, 10/2025
// Description : Structured tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/structured.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
Structured::Structured(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kStructured, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
}

//------------------------------------------------------------------------
Structured::Structured(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kStructured, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void Structured::tweakConfigBeforeCreatingVoc()
{
    config.useChords         = false;
    config.useRests          = false;
    config.useTempos         = false;
    config.useTimeSignatures = false;
    config.useSustainPedals  = false;
    config.usePitchBends     = false;
    config.usePitchIntervals = false;
    config.programChanges    = false;

    disableAttributeControls();
}

//------------------------------------------------------------------------
std::vector<Event> Structured::createTrackEvents(
    const TrackType &track, const std::vector<std::pair<int, int>> &_, symusic::i32 __, const std::vector<int> &___,
    const std::vector<int> &____, const std::unordered_map<int, std::variant<std::vector<int>, bool>> &_____) const
{
    // Make sure the notes are sorted first by their onset (start) times, second by
    // pitch: notes are sorted in preprocessScore.
    int program       = !track.is_drum ? track.program : -1;
    bool useDurations = config.useNoteDurationPrograms.find(program) != config.useNoteDurationPrograms.end();
    std::vector<Event> events;

    // Create the NoteOn, NoteOff and Velocity events
    int previousTick = 0;
    int ticksPerBeat = this->timeDivision;

    for (const NoteType &note : *track.notes)
    {
        // In this case, we directly add TimeShift events here so we don't have to
        // call addTimeNoteEvents and avoid delay caused by event sorting
        if (!config.oneTokenStreamForPrograms)
        {
            int timeShiftTicks = note.start() - previousTick;
            std::string timeShift;
            if (timeShiftTicks != 0)
            {
                timeShiftTicks = libmusictokUtils::npGetClosest(tpbToTimeArray.at(ticksPerBeat), timeShiftTicks);
                timeShift      = tpbTicksToTokens.at(ticksPerBeat).at(timeShiftTicks);
            }
            else
            {
                timeShift = "0.0.1";
            }
            events.emplace_back(/*type*/ "TimeShift",
                                /*value*/ timeShift,
                                /*time*/ note.start(),
                                /*prorgam*/ 0,
                                /*desc*/ std::to_string(timeShiftTicks) + " ticks");
        }
        // NoteOn / Velocity / Duration
        if (config.usePrograms)
        {
            events.emplace_back(/*type*/ "Program",
                                /*value*/ program,
                                /*time*/ note.start(),
                                /*prorgam*/ 0,
                                /*desc*/ note.end());
        }
        std::string pitchTokenName = track.is_drum && config.usePitchdrumTokens ? "PitchDrum" : "Pitch";
        events.emplace_back(/*type*/ pitchTokenName,
                            /*value*/ note.pitch,
                            /*time*/ note.start(),
                            /*prorgam*/ 0,
                            /*desc*/ note.end());
        if (config.useVelocities)
        {
            events.emplace_back(/*type*/ "Velocity",
                                /*value*/ note.velocity,
                                /*time*/ note.start(),
                                /*prorgam*/ 0,
                                /*desc*/ note.velocity);
        }
        if (useDurations)
        {
            std::string dur = tpbTicksToTokens.at(ticksPerBeat).at(note.duration);
            events.emplace_back(/*type*/ "Duration",
                                /*value*/ dur,
                                /*time*/ note.start(),
                                /*prorgam*/ 0,
                                /*desc*/ std::to_string(note.duration) + " ticks");
        }
        previousTick = note.start();
    }
    return events;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> Structured::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
{
    std::vector<Event> allEvents;
    std::vector<std::string> tokenTypesToCheck;
    if (config.oneTokenStreamForPrograms)
    {
        tokenTypesToCheck = {"Program"};
    }
    else
    {
        tokenTypesToCheck = {"Pitch", "PitchDrum"};
    }

    // Add timeShift tokens before each pitch token
    int previousTick = 0;
    for (const Event &event : events)
    {
        bool insertTimeEventNow = false;
        for (const std::string &tokType : tokenTypesToCheck)
        {
            if (tokType == event.type)
            {
                insertTimeEventNow = true;
                break;
            }
        }
        if (insertTimeEventNow)
        {
            // TimeShift
            int timeShiftTicks = event.time - previousTick;
            std::string timeShift;
            if (timeShiftTicks != 0)
            {
                timeShiftTicks = libmusictokUtils::npGetClosest(tpbToTimeArray.at(timeDivision), timeShiftTicks);
                timeShift      = tpbTicksToTokens.at(timeDivision).at(timeShiftTicks);
            }
            else
            {
                timeShift = "0.0.1";
            }
            allEvents.emplace_back(/*type*/ "TimeShift",
                                   /*value*/ timeShift,
                                   /*time*/ event.time,
                                   /*prorgam*/ 0,
                                   /*desc*/ std::to_string(timeShiftTicks) + " ticks");
            previousTick = event.time;
        }
        allEvents.push_back(event);
    }
    return allEvents;
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> Structured::scoreToTokens(
    const ScoreType &score, const AttributeControlsIndexesType &_) const
{
    std::vector<Event> allEventsVec;
    std::vector<std::vector<Event>> allEventsVecVec;

    // Add note tokens
    if (!config.oneTokenStreamForPrograms && score.tracks->size() == 0)
    {
        allEventsVecVec.push_back({});
    }
    for (std::shared_ptr<TrackType> track : *score.tracks)
    {
        std::vector<Event> noteEvents = createTrackEvents(*track);
        if (config.oneTokenStreamForPrograms)
        {
            allEventsVec.insert(allEventsVec.end(), noteEvents.begin(), noteEvents.end());
        }
        else
        {
            allEventsVecVec.push_back(noteEvents);
        }
    }

    // Add time events
    if (config.oneTokenStreamForPrograms)
    {
        if (score.tracks->size() > 1)
        {
            sortEvents(allEventsVec);
        }
        allEventsVec = std::get<std::vector<Event>>(addTimeEvents(allEventsVec, score.ticks_per_quarter));
        TokSequence tokSeq;
        tokSeq.events = allEventsVec;
        complete_sequence(tokSeq);
        return tokSeq;
    }
    else
    {
        std::vector<TokSequence> tokSeqVec;
        for (size_t i = 0; i < allEventsVecVec.size(); i++)
        {
            // No call to addTimeEvents as not needed
            TokSequence tokSeq;
            tokSeq.events = allEventsVecVec.at(i);
            complete_sequence(tokSeq);
            tokSeqVec.push_back(tokSeq);
        }
        return tokSeqVec;
    }
}

//------------------------------------------------------------------------
ScoreType Structured::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
                                    const std::vector<std::tuple<int, bool>> &programs) const
{
    // unsqueeze tokens in case of oneTokenStream
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

    std::map<int, TrackType> instruments;
    int currentTick    = 0;
    int currentProgram = 0;
    int ticksPerBeat   = score.ticks_per_quarter;

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::string> &seq = tokens[si];

        TrackType currentTrack;
        bool currentTrackUseDuration = false;

        if (!config.oneTokenStreamForPrograms)
        {
            currentTick = 0;
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

            if (tokType == "TimeShift" && tokVal != "0.0.1")
            {
                currentTick += tpbTokensToTicks.at(ticksPerBeat).at(tokVal);
            }
            else if (tokType == "Pitch" || tokType == "PitchDrum")
            {
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
                        int pitch = std::stoi(libmusictokUtils::split(seq.at(ti), "_").at(1));
                        if (currentTrackUseDuration)
                        {
                            dur = tpbTokensToTicks.at(ticksPerBeat).at(durSplit[1]);
                        }
                        NoteType newNote(currentTick, dur, pitch, vel);
                        if (config.oneTokenStreamForPrograms)
                        {
                            libmusictokUtils::Internal::checkInst(currentProgram, instruments);
                            instruments[currentProgram]->notes->append(newNote);
                        }
                        else
                        {
                            currentTrack->notes->append(newNote);
                        }
                    }
                }
                catch (const std::out_of_range &)
                {
                    // A well constituted sequence should not raise an exception
                    // However with generated sequences this can happen, or if the
                    // sequence isn't finished
                    std::clog << "bad-sequence caught and let slip in Structured::tokensToScore." << std::endl;
                }
            }
            else if (tokType == "Program")
            {
                currentProgram = std::stoi(tokVal);
                currentTrackUseDuration =
                    (std::find(config.useNoteDurationPrograms.begin(), config.useNoteDurationPrograms.end(),
                               currentProgram) != config.useNoteDurationPrograms.end());
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
        for (const auto &prgTrack : instruments)
        {
            score.tracks->push_back(std::make_shared<TrackType>(prgTrack.second));
        }
    }

    return score;
}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> Structured::createBaseVocabulary()
{
    std::vector<std::string> vocab;

    // NoteOn/NoteOff/Velocity
    addNoteTokensToVocabList(vocab);

    // TimeShift
    vocab.push_back("TimeShift_0.0.1");
    for (std::tuple<int, int, int> &dur : durations)
    {
        vocab.push_back("TimeShift_" + libmusictokUtils::join(dur, "."));
    }

    addAdditionalTokensToVocabList(vocab);

    return vocab;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> Structured::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> dic;
    if (config.useVelocities)
    {
        dic["Pitch"]     = {"Velocity"};
        dic["PitchDrum"] = {"Velocity"};
    }
    else if (config.usingNoteDurationTokens())
    {
        dic["Pitch"]     = {"Duration"};
        dic["PitchDrum"] = {"Duration"};
    }
    else
    {
        dic["Pitch"]     = {"TimeShift"};
        dic["PitchDrum"] = {"TimeShift"};
    }

    dic["TimeShift"] = {"Pitch", "PitchDrum"};

    if (config.useVelocities)
    {
        if (config.usingNoteDurationTokens())
        {
            dic["Velocity"] = {"Duration"};
        }
        else
        {
            dic["Velocity"] = {"TimeShift"};
        }
    }
    if (config.usingNoteDurationTokens())
    {
        dic["Duration"] = {"TimeShift"};
    }
    if (config.usePrograms)
    {
        dic["Program"]   = {"Pitch", "PitchDrum"};
        dic["TimeShift"] = {"Program"};
    }
    return dic;
}

} // namespace libmusictok
