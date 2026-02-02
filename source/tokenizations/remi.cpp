//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/remi.cpp
// Created by  : Steinberg, 05/2025
// Description : REMI tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/remi.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
REMI::REMI(const std::filesystem::path &tokenizerFile, int maxBarEmbedding, bool v)
    : MusicTokenizer(TokenizerType::kREMI, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();

    if (maxBarEmbedding != -1 && config.additionalParams.find("max_bar_embedding") != config.additionalParams.end())
    {
        config.additionalParams["max_bar_embedding"] = maxBarEmbedding;
    }
}

//------------------------------------------------------------------------
REMI::REMI(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kREMI, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void REMI::tweakConfigBeforeCreatingVoc()
{
    if (config.additionalParams.find("max_bar_embedding") == config.additionalParams.end())
    {
        config.additionalParams["max_bar_embedding"] = nullptr;
    }
    if (config.additionalParams.find("use_bar_end_tokens") == config.additionalParams.end())
    {
        config.additionalParams["use_bar_end_tokens"] = useBarEndTokens_default;
    }
    if (config.additionalParams.find("add_trailing_bars") == config.additionalParams.end())
    {
        config.additionalParams["add_trailing_bars"] = addTrailingBars_default;
    }
}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> REMI::createBaseVocabulary()
{
    std::vector<std::string> vocab;

    // Bar
    if (config.additionalParams["max_bar_embedding"] != nullptr)
    {
        for (size_t i = 0; i < config.additionalParams["max_bar_embedding"].get<int>(); ++i)
        {
            vocab.push_back("Bar_" + std::to_string(i));
        }
    }
    else
    {
        vocab.push_back("Bar_None");
    }
    if (config.additionalParams["use_bar_end_tokens"])
    {
        vocab.push_back("Bar_End");
    }

    // NoteOn/NoteOff/Velocity
    addNoteTokensToVocabList(vocab);

    // Position
    // time_division is equal to the maximum possible ticks/beat value.
    int maxNumBeats = 0;
    for (const auto &ts : this->timeSignatures)
    {
        if (ts.first > maxNumBeats)
        {
            maxNumBeats = ts.first;
        }
    }
    int numPositions = config.maxNumPosPerBeat() * maxNumBeats;
    for (size_t i = 0; i < numPositions; ++i)
    {
        vocab.push_back("Position_" + std::to_string(i));
    }

    addAdditionalTokensToVocabList(vocab);

    return vocab;
}

//------------------------------------------------------------------------
int REMI::computeTicksPerPos(int ticksPerBeat) const
{
    return ticksPerBeat / config.maxNumPosPerBeat();
}

//------------------------------------------------------------------------
std::tuple<int, int, int> REMI::computeTicksPerUnits(symusic::Tick::unit time,
                                                     std::pair<symusic::u8, symusic::u8> currentTimeSig,
                                                     int timeDivision) const
{
    int ticksPerBar  = libmusictokUtils::computeTicksPerBar(currentTimeSig, timeDivision);
    int ticksPerBeat = libmusictokUtils::computeTicksPerBeat(currentTimeSig.second, timeDivision);
    int ticksPerPos  = computeTicksPerPos(ticksPerBeat);
    return {ticksPerBar, ticksPerBeat, ticksPerPos};
}

//------------------------------------------------------------------------
int unitsBetween(int startTick, int endTick, int ticksPerUnit)
{
    return (endTick - startTick) / ticksPerUnit;
}

//------------------------------------------------------------------------
std::tuple<int, int> REMI::addNewBars(int untilTime, const std::string &eventType, std::vector<Event> &allEvents,
                                      int currentBar, int barAtLastTsChange, int tickAtLastTsChange,
                                      int tickAtCurrentBar, std::pair<symusic::u8, symusic::u8> currentTimeSig,
                                      int ticksPerBar) const
{
    int numNewBars = barAtLastTsChange + unitsBetween(tickAtLastTsChange, untilTime, ticksPerBar) - currentBar;

    int tickAtCurrentBar_ = tickAtCurrentBar;
    for (size_t i = 0; i < numNewBars; ++i)
    {
        ++currentBar;
        tickAtCurrentBar_ = tickAtLastTsChange + ((currentBar - barAtLastTsChange) * ticksPerBar);

        if (config.additionalParams.at("use_bar_end_tokens").get<bool>() && currentBar > 0)
        {
            allEvents.push_back(Event("Bar", "End", tickAtCurrentBar_ - 1));
        }
        allEvents.push_back(Event(
            "Bar", !config.additionalParams.at("max_bar_embedding").is_null() ? std::to_string(currentBar + i) : "None",
            tickAtCurrentBar_));
        if (config.useTimeSignatures && !(eventType == "TimeSig" && i + 1 == numNewBars))
        {
            allEvents.push_back(
                Event("TimeSig", std::to_string(currentTimeSig.first) + "/" + std::to_string(currentTimeSig.second),
                      tickAtCurrentBar_));
        }
    }
    return {currentBar, tickAtCurrentBar_};
}

//------------------------------------------------------------------------
void REMI::addPositionEvent(Event event, std::vector<Event> &allEvents, int tickAtCurrentBar, int ticksPerPos) const
{
    int posIndex = unitsBetween(tickAtCurrentBar, event.time, ticksPerPos);
    allEvents.push_back(Event("Position", posIndex, event.time, 0, event.time));
}

//------------------------------------------------------------------------
int previousNoteEndUpdate(const Event &event, int previousNoteEnd)
{
    int eventTime = 0;
    if (event.type == "Pitch" || event.type == "PitchDrum" || event.type == "PitchIntervalTime" ||
        event.type == "PitchIntervalChord")
    {
        assert(std::holds_alternative<int>(event.desc));
        eventTime = std::get<int>(event.desc);
    }
    else if (event.type == "Program" || event.type == "Tempo" || event.type == "TimeSig" || event.type == "Pedal" ||
             event.type == "PedalOff" || event.type == "PitchBend" || event.type == "Chord")
    {
        eventTime = event.time;
    }
    return std::max(previousNoteEnd, eventTime);
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> REMI::addTimeEvents(const std::vector<Event> &events,
                                                                                      symusic::i32 timeDivision) const
{
    std::vector<Event> allEvents;
    int currentBar         = -1;
    int barAtLastTsChange  = 0;
    int previousTick       = -1;
    int previousNoteEnd    = 0;
    int tickAtLastTsChange = 0;
    int tickAtCurrentBar   = 0;

    // Determine time signature and compute ticks per entites
    std::pair<int, int> currentTimeSig = timeSignature_default;
    int timeSigTime                    = 0;

    // Check for TimeSig token at tick 0
    if (config.useTimeSignatures)
    {
        for (const auto &event : events)
        {
            if (event.type == "TimeSig")
            {
                currentTimeSig = parseTokenTimeSignature(std::get<std::string>(event.value));
                timeSigTime    = event.time;
                break;
            }
        }
    }
    int ticksPerBar, ticksPerBeat, ticksPerPos;
    std::tie(ticksPerBar, ticksPerBeat, ticksPerPos) = computeTicksPerUnits(timeSigTime, currentTimeSig, timeDivision);

    for (size_t ei = 0; ei < events.size(); ++ei)
    {
        auto &event = events[ei];
        if (startsWith(event.type, "ACTrack"))
        {
            allEvents.push_back(event);
            continue;
        }
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
                // Update currentBar and tickAtCurrentBar without creating Bar tokens
                int realCurrentBar = barAtLastTsChange + unitsBetween(tickAtLastTsChange, previousTick, ticksPerBar);
                if (realCurrentBar > currentBar)
                {
                    if (currentBar == -1)
                    {
                        currentBar = 0;
                    }
                    tickAtCurrentBar += (realCurrentBar - currentBar) * ticksPerBar;
                    currentBar = realCurrentBar;
                }
            }
            // Bar
            std::tie(currentBar, tickAtCurrentBar) =
                addNewBars(event.time, event.type, allEvents, currentBar, barAtLastTsChange, tickAtLastTsChange,
                           tickAtCurrentBar, currentTimeSig, ticksPerBar);

            // Position
            if (event.type != "TimeSig" && !startsWith(event.type, "ACBar"))
                addPositionEvent(event, allEvents, tickAtCurrentBar, ticksPerPos);

            previousTick = event.time;
        }
        // Update time signature time variables
        if (event.type == "TimeSig")
        {
            barAtLastTsChange += unitsBetween(tickAtLastTsChange, event.time, ticksPerBar);
            tickAtLastTsChange = event.time;
            currentTimeSig     = parseTokenTimeSignature(std::get<std::string>(event.value));
            std::tie(ticksPerBar, ticksPerBeat, ticksPerPos) =
                computeTicksPerUnits(event.time, currentTimeSig, timeDivision);
            previousTick -= 1;
        }

        allEvents.push_back(event);

        // Add Position if current event is bar-level control & next one is at the same pos
        if (startsWith(event.type, "ACBar") && ei + 1 < events.size() && !startsWith(events[ei + 1].type, "ACBar") &&
            event.time == events[ei + 1].time)
        {
            addPositionEvent(event, allEvents, tickAtCurrentBar, ticksPerPos);
        }

        previousNoteEnd = previousNoteEndUpdate(event, previousNoteEnd);
    }

    // Add trailing bars if necessary
    if (previousNoteEnd > previousTick && config.additionalParams.at("add_trailing_bars").get<bool>())
    {
        int unused;
        std::tie(unused, std::ignore) =
            addNewBars(previousNoteEnd, events.back().type, allEvents, currentBar, barAtLastTsChange,
                       tickAtLastTsChange, tickAtCurrentBar, currentTimeSig, ticksPerBar);
    }

    return allEvents;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> REMI::createTokensTypesGraph()
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
        dic["Velocity"] = config.usingNoteDurationTokens()
                              ? std::set<std::string>{"Duration"}
                              : std::set<std::string>{firstNoteTokenType, "Position", "Bar"};
    }
    else if (config.usingNoteDurationTokens())
    {
        dic["Pitch"] = {"Duration"};
    }
    else
    {
        dic["Pitch"] = {firstNoteTokenType, "Bar", "Position"};
    }

    if (config.usingNoteDurationTokens())
    {
        dic["Duration"] = {firstNoteTokenType, "Position", "Bar"};
    }
    dic["Bar"]      = {"Position", "Bar"};
    dic["Position"] = {firstNoteTokenType};

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
                dic[tokenType] = {firstNoteTokenType, "PitchIntervalTime", "PitchIntervalChord", "Bar", "Position"};
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
                dic["Position"].insert(tokenType);
            }
        }
    }

    if (config.programChanges)
    {
        auto &t = dic[(config.usingNoteDurationTokens() ? "Duration"
                                                        : (config.useVelocities ? "Velocity" : firstNoteTokenType))];
        t.insert("Program");
        // The first bar may be empty but the Program token will still be present
        if (config.additionalParams.at("use_bar_end_tokens"))
        {
            dic["Program"].insert("Bar");
        }
    }

    if (config.useChords)
    {
        dic["Chord"] = {firstNoteTokenType};
        dic["Position"].insert("Chord");
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
        dic["Position"].insert("Tempo");
        dic["Tempo"] = {firstNoteTokenType, "Position", "Bar"};
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
        dic["Bar"] = {"TimeSig"};
        if (config.additionalParams.at("use_bar_end_tokens"))
        {
            dic["Bar"].insert("Bar");
        }
        dic["TimeSig"] = {firstNoteTokenType, "Position", "Bar"};
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
            dic["Tempo"].insert("TimeSig");
        }
        if (config.usePitchIntervals)
        {
            dic["TimeSig"].insert("PitchIntervalTime");
            dic["TimeSig"].insert("PitchIntervalChord");
        }
    }

    if (config.useSustainPedals)
    {
        dic["Position"].insert("Pedal");
        if (config.sustainPedalDuration)
        {
            dic["Pedal"] = {"Duration"};
            if (config.usingNoteDurationTokens())
            {
                dic["Duration"].insert("Pedal");
            }
            else if (config.useVelocities)
            {
                dic["Duration"] = {firstNoteTokenType, "Bar", "Position"};
                dic["Velocity"].insert("Pedal");
            }
            else
            {
                dic["Duration"] = {firstNoteTokenType, "Bar", "Position"};
                dic["Pitch"].insert("Pedal");
            }
        }
        else
        {
            dic["PedalOff"] = {"Pedal", "PedalOff", firstNoteTokenType, "Position", "Bar"};
            dic["Pedal"]    = {"Pedal", firstNoteTokenType, "Position", "Bar"};
            dic["Position"].insert("PedalOff");
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
        dic["PitchBend"] = {firstNoteTokenType, "Position", "Bar"};
        if (config.usePrograms && !config.programChanges)
        {
            dic["Program"].insert("PitchBend");
        }
        else
        {
            dic["Position"].insert("PitchBend");
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
        dic["Rest"] = {"Rest", firstNoteTokenType, "Position", "Bar"};
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

    if (config.programChanges)
    {
        for (const auto &tokenType :
             {"Position", "Rest", "PitchBend", "Pedal", "PedalOff", "Tempo", "TimeSig", "Chord"})
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

//------------------------------------------------------------------------
ScoreType REMI::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
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

        // Handle time signatures (only for si == 0)
        if (si == 0)
        {
            if (config.useTimeSignatures)
            {
                for (const std::string &token : seq)
                {
                    auto tokParts       = libmusictokUtils::split(token, "_");
                    std::string tokType = tokParts[0];
                    std::string tokVal  = tokParts[1];
                    if (tokType == "TimeSig")
                    {
                        auto [num, den] = parseTokenTimeSignature(tokVal);
                        timeSignatureChanges.emplace_back(0, num, den);
                        break;
                    }
                    if (tokType == "Pitch" || tokType == "PitchDrum" || tokType == "Velocity" ||
                        tokType == "Duration" || tokType == "PitchBend" || tokType == "Pedal")
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
        auto currentTimeSig = timeSignatureChanges.back();
        int ticksPerBar     = libmusictokUtils::computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
        int ticksPerBeat    = tpbPerTs.at(currentTimeSig.denominator);
        int ticksPerPos     = computeTicksPerPos(ticksPerBeat);

        // Tracking variables
        int currentTick        = 0;
        int tickAtLastTsChange = 0;
        int tickAtCurrentBar   = 0;
        int currentBar         = -1;
        int barAtLastTsChange  = 0;
        int currentProgram     = 0;
        int previousNoteEnd    = 0;
        std::unordered_map<int, int> previousPitchOnset, previousPitchChord;
        for (int prog : config.programs)
        {
            previousPitchOnset[prog] = -128;
            previousPitchChord[prog] = -128;
        }
        std::unordered_map<int, int> activePedals;

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

            if (seq[ti] == "Bar_None")
            {
                currentBar++;
                if (currentBar > 0)
                    currentTick = tickAtCurrentBar + ticksPerBar;
                tickAtCurrentBar = currentTick;
            }
            else if (tokType == "Rest")
            {
                currentTick = std::max(previousNoteEnd, currentTick);
                currentTick += tpbRestsToTicks.at(ticksPerBeat).at(tokVal);
                int realCurrentBar = barAtLastTsChange + unitsBetween(tickAtLastTsChange, currentTick, ticksPerBar);
                if (realCurrentBar > currentBar)
                {
                    if (currentBar == -1)
                    {
                        currentBar = 0;
                    }
                    tickAtCurrentBar += (realCurrentBar - currentBar) * ticksPerBar;
                    currentBar = realCurrentBar;
                }
            }
            else if (tokType == "Position")
            {
                if (currentBar == -1)
                {
                    currentBar = 0;
                }
                currentTick = tickAtCurrentBar + std::stoi(tokVal) * ticksPerPos;
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
                    std::clog << "bad-sequence caught and let slip in REMI::tokensToScore." << std::endl;
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
            else if (tokType == "Tempo")
            {
                if (si == 0)
                {
                    tempoChanges.emplace_back(currentTick, TempoType::qpm2mspq(std::stof(tokVal)));
                }
                previousNoteEnd = std::max(previousNoteEnd, currentTick);
            }
            else if (tokType == "TimeSig")
            {
                auto [num, den] = parseTokenTimeSignature(tokVal);
                if (num != currentTimeSig.numerator || den != currentTimeSig.denominator)
                {
                    currentTimeSig = TimeSigType(currentTick, num, den);
                    if (si == 0)
                    {
                        timeSignatureChanges.push_back(currentTimeSig);
                    }
                    tickAtLastTsChange = tickAtCurrentBar;
                    barAtLastTsChange  = currentBar;
                    ticksPerBar        = libmusictokUtils::computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
                    ticksPerBeat       = tpbPerTs.at(den);
                    ticksPerPos        = computeTicksPerPos(ticksPerBeat);
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
                        tracks[pedalProg].pedals->push_back(newPedal);
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
        if (!config.oneTokenStreamForPrograms && !libmusictokUtils::Internal::isTrackEmpty(currentTrack))
        {
            score.tracks->push_back(std::make_shared<TrackType>(currentTrack));
        }
    }

    // Add global events to the score
    if (config.oneTokenStreamForPrograms)
    {
        for (auto &prgTrack : tracks)
        {
            score.tracks->push_back(std::make_shared<TrackType>(prgTrack.second));
        }
    }

    score.tempos          = std::make_shared<symusic::pyvec<TempoType>>(tempoChanges);
    score.time_signatures = std::make_shared<symusic::pyvec<TimeSigType>>(timeSignatureChanges);
    return score;
}

} // namespace libmusictok
