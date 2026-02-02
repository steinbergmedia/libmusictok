//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/pertok.cpp
// Created by  : Steinberg, 05/2025
// Description : PerTok tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/pertok.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
PerTok::PerTok(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kPerTok, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();

    postConstructorChecks();
}

//------------------------------------------------------------------------
PerTok::PerTok(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kPerTok, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();

    postConstructorChecks();
}

//------------------------------------------------------------------------
void PerTok::postConstructorChecks()
{
    if (config.additionalParams.find("ticks_per_quarter") == config.additionalParams.end())
    {
        std::invalid_argument("Tokenizer config must have a value for ticks_per_quarter");
    }

    if (useMicrotiming && microtimingTickValues.empty())
    {
        microtimingTickValues = createMicrotimingTickValues();
    }
}

//------------------------------------------------------------------------
void PerTok::tweakConfigBeforeCreatingVoc()
{
    if (config.additionalParams.find("ticks_per_quarter") == config.additionalParams.end())
    {
        std::runtime_error("Tokenizer config must have a value for ticks_per_quarter");
    }
    tpq = config.additionalParams.at("ticks_per_quarter").get<int>();

    if (config.additionalParams.find("use_microtiming") != config.additionalParams.end())
    {
        useMicrotiming = config.additionalParams.at("use_microtiming").get<bool>();
    }

    if (useMicrotiming)
    {
        std::vector<std::string> missing;
        if (config.additionalParams.find("max_microtiming_shift") == config.additionalParams.end())
            missing.push_back("max_microtiming_shift");
        if (config.additionalParams.find("num_microtiming_bins") == config.additionalParams.end())
            missing.push_back("num_microtiming_bins");

        if (!missing.empty())
        {
            std::string missingAsString;
            for (size_t i = 0; i < missing.size(); ++i)
            {
                missingAsString += missing[i];
                if (i != missing.size() - 1)
                    missingAsString += ", ";
            }
            std::string msg = "TokenizerConfig is missing required keys: " + missingAsString;
            throw std::invalid_argument(msg);
        }
    }

    maxMtShift = config.additionalParams.at("max_microtiming_shift").get<double>() * tpq;

    usePositionToks = false;
    if (config.additionalParams.find("use_position_toks") != config.additionalParams.end())
    {
        usePositionToks = config.additionalParams.at("use_position_toks").get<bool>();
    }
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> PerTok::addTimeEvents(
    const std::vector<Event> &events, symusic::i32 timeDivision) const
{
    std::vector<Event> allEvents;
    int previousTick  = 0;
    int ticksPerBar   = tpq * timeSignature_default.first;
    int currBar       = 0;
    int barTime       = 0;
    int pos           = 0;
    int positionInBar = 0;
    int microtiming   = 0;

    for (const Event &event : events)
    {
        // Bar
        int globalTime = previousTick;
        while (event.time > ((currBar + 1) * ticksPerBar - minTimeShift))
        {
            globalTime += ticksPerBar - (globalTime % ticksPerBar);
            barTime += ticksPerBar;
            positionInBar = 0;
            allEvents.emplace_back("Bar", "None", barTime, 0, "Bar " + std::to_string(barTime));
            currBar++;
            previousTick = currBar * ticksPerBar;
        }

        // TimeSig
        if (event.type == "TimeSig")
        {
            auto [num, den] = parseTokenTimeSignature(std::get<std::string>(event.value));
            ticksPerBar     = static_cast<double>(den) / 4.0 * num * tpq;
        }

        int timeDelta = event.time - previousTick;

        if (std::find(microTimeEvents.begin(), microTimeEvents.end(), event.type) != microTimeEvents.end())
        {
            // Option 1: adding position tokens
            if (usePositionToks)
            {
                positionInBar = event.time - barTime;
                pos           = findClosestPositionTok(positionInBar);
                microtiming   = positionInBar - pos;
                if (timeDelta >= minTimeShift)
                {
                    allEvents.emplace_back("Position", pos, event.time, 0, "position " + std::to_string(pos));
                    previousTick = barTime + pos;
                }
            }
            // Option 2: creating timeShift tokens
            else
            {
                int timeShift = 0;

                if (timeDelta >= minTimeShift)
                {
                    std::tuple<int, int, int> tsTuple = getClosestDurationTuple(timeDelta);
                    std::string ts                    = libmusictokUtils::join(tsTuple, ".");

                    allEvents.emplace_back("TimeShift", ts, event.time, 0, "timeshift " + ts);

                    timeShift = std::get<0>(tsTuple) * std::get<2>(tsTuple) + std::get<1>(tsTuple);
                    previousTick += timeShift;
                }
                microtiming = timeDelta - timeShift;
            }
        }

        allEvents.push_back(event);

        // Microtiming
        // These will come after certain types of tokens, e.g. Pitch
        if (useMicrotiming &&
            std::find(microTimeEvents.begin(), microTimeEvents.end(), event.type) != microTimeEvents.end())
        {
            int closestMicrotiming = getClosestArrayValue(microtiming, microtimingTickValues);
            allEvents.emplace_back("MicroTiming", closestMicrotiming, event.time, 0,
                                   std::to_string(closestMicrotiming) + "microtiming");
        }
    }
    return allEvents;
}

//------------------------------------------------------------------------
ScoreType PerTok::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens_,
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

    ScoreType score = ScoreType(this->tpq);
    int mtOffset    = useMicrotiming ? 1 : 0;
    int velOffset   = config.useVelocities ? mtOffset + 1 : mtOffset;
    int durOffset   = velOffset + 1;

    // Result
    std::map<int, TrackType> tracks;
    symusic::pyvec<TempoType> tempoChanges;
    symusic::pyvec<TimeSigType> timeSignatureChanges;

    TrackType currentTrack;
    int ticksPerBeat = score.ticks_per_quarter;

    // Loop over token sequences (tracks)
    for (size_t si = 0; si < tokens.size(); ++si)
    {
        std::vector<std::string> &seq = tokens.at(si);

        // Set tracking variables
        int currentTick    = 0;
        int currBar        = 0;
        int currBarTick    = 0;
        int currentProgram = 0;
        std::unordered_map<int, int> activePedals;
        int ticksPerBar = ticksPerBeat * timeSignature_default.first;
        std::unordered_map<int, int> previousPitchOnset, previousPitchChord;
        for (int prog : config.programs)
        {
            previousPitchOnset[prog] = -128;
            previousPitchChord[prog] = -128;
        }
        bool currentTrackUseDuration = false;
        // Set track / sequence program if needed
        if (!config.oneTokenStreamForPrograms)
        {
            bool isDrum = false;
            if (!programs.empty())
            {
                std::tie(currentProgram, isDrum) = programs.at(si);
            }
            else if (config.usePrograms)
            {
                for (const std::string &token : seq)
                {
                    const auto parts = libmusictokUtils::split(token, "_");
                    if (parts.at(0).rfind("Program", 0) == 0)
                    {
                        currentProgram = std::stoi(parts.at(1));
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

        // Decode Tokens
        for (size_t ti = 0; ti < seq.size(); ++ti)
        {
            const std::string &token = seq.at(ti);

            std::vector<std::string> parts = libmusictokUtils::split(token, "_");
            std::string tokType            = parts.at(0);
            std::string tokVal             = parts.at(1);

            if (tokType == "Bar")
            {
                currBar++;
                currentTick += (ticksPerBar * currBar) - currentTick;
                currBarTick = currentTick;
            }
            else if (tokType == "TimeShift")
                currentTick += convertDurationsToTicks(tokVal);

            else if (tokType == "Position")
                currentTick += currBarTick + std::stoi(tokVal);

            else if (tokType == "Pitch" || tokType == "PitchDrum" || tokType == "PitchIntervalTime" ||
                     tokType == "PitchIntervalChord")
            {
                int pitch;

                if (tokType == "Pitch" || tokType == "PitchDrum")
                    pitch = std::stoi(tokVal);
                else if (tokType == "PitchIntervalTime")
                    pitch = previousPitchOnset.at(currentProgram) + std::stoi(tokVal);
                else
                    pitch = previousPitchChord.at(currentProgram) + std::stoi(tokVal);

                if (pitch < config.pitchRange.first || pitch > config.pitchRange.second)
                {
                    continue;
                }

                if (tokType != "PitchIntervalChord")
                {
                    previousPitchOnset.at(currentProgram) = pitch;
                }
                previousPitchChord.at(currentProgram) = pitch;

                try
                {
                    std::vector<std::string> mtSplit;
                    int mt;
                    std::vector<std::string> velSplit;
                    std::string velType;
                    int vel;
                    std::vector<std::string> durSplit;
                    std::string durType;
                    int dur;

                    mtSplit = libmusictokUtils::split(seq.at(ti + mtOffset), "_");
                    if (useMicrotiming && mtSplit[0] == "MicroTiming")
                    {
                        mt = std::stoi(mtSplit[1]);
                    }
                    else
                    {
                        mt = 0;
                    }

                    if (config.useVelocities)
                    {
                        velSplit = libmusictokUtils::split(seq.at(ti + velOffset), "_");
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
                            dur = convertDurationsToTicks(durSplit[1]);
                        }
                        mt += currentTick;
                        NoteType newNote(mt, dur, pitch, vel);
                        if (config.oneTokenStreamForPrograms)
                        {
                            libmusictokUtils::Internal::checkInst(currentProgram, tracks);
                            tracks[currentProgram]->notes->append(newNote);
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
            }
            else if (tokType == "TimeSig")
            {
                const auto &[num, den] = parseTokenTimeSignature(tokVal);
                ticksPerBar =
                    static_cast<int>(static_cast<double>(den) / 4.0 * static_cast<double>(num * ticksPerBeat));
                if (si == 0)
                {
                    timeSignatureChanges.emplace_back(currentTick, num, den);
                }
            }
            else if (tokType == "Pedal")
            {
                int pedalProg = config.usePrograms ? std::stoi(tokVal) : currentProgram;
                if (config.sustainPedalDuration && ti + 1 < seq.size())
                {
                    const std::vector<std::string> nextParts = libmusictokUtils::split(seq[ti + 1], "_");
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

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> PerTok::createBaseVocabulary()
{
    std::vector<std::string> vocab = {"Bar_None"};

    // NoteOn / NoteOff / Velocity
    timeshiftTickValues = createTimeshiftTickValues();
    addNoteTokensToVocabList(vocab);

    //Position tokens - for denoting absolute positions of tokens
    if (usePositionToks)
    {
        positionLocations = createPositionTokLocations();
        for (int pos : positionLocations)
        {
            vocab.push_back("Position_" + std::to_string(pos));
        }
    }
    else
    {
        // Timeshift
        for (const std::tuple<int, int, int> &dur : durations)
        {
            vocab.push_back("TimeShift_" + libmusictokUtils::join(dur, "."));
        }
    }
    // Duration
    bool isUsingNoteDurProgs = std::any_of(config.useNoteDurationPrograms.begin(), config.useNoteDurationPrograms.end(),
                                           [&](int p) { return p > 0; });
    if (isUsingNoteDurProgs)
    {
        for (const std::tuple<int, int, int> &dur : durations)
        {
            vocab.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
    }

    // Microtiming
    if (useMicrotiming)
    {
        microtimingTickValues = createMicrotimingTickValues();

        for (int mt : microtimingTickValues)
        {
            vocab.push_back("MicroTiming_" + std::to_string(mt));
        }
    }

    // add additional tokens
    addAdditionalTokensToVocabList(vocab);

    return libmusictokUtils::removeDuplicatesPreserveOrder(vocab);
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> PerTok::createTokensTypesGraph()
{
    return std::unordered_map<std::string, std::set<std::string>>();
}

//------------------------------------------------------------------------
int PerTok::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    return 0;
}

//------------------------------------------------------------------------
ScoreType PerTok::resampleScore(const ScoreType &score, int newTpq,
                                symusic::pyvec<TimeSigType> &timeSignaturesCopy) const
{
    if (score.ticks_per_quarter != tpq)
    {
        ScoreType resampledScore = symusic::resample(score, tpq, 1);
        return resampledScore;
    }

    return score;
}

//------------------------------------------------------------------------
void PerTok::adjustDurations(symusic::pyvec<NoteType> &notes,
                             const std::vector<std::pair<int, int>> &ticksPerBeat) const
{
    // Do nothing
}

//------------------------------------------------------------------------
void PerTok::adjustDurations(symusic::pyvec<PedalType> &pedals,
                             const std::vector<std::pair<int, int>> &ticksPerBeat) const
{
    // Do nothing
}

//------------------------------------------------------------------------
std::tuple<int, int, int> PerTok::getClosestDurationTuple(int target) const
{
    return *std::min_element(this->durations.begin(), this->durations.end(),
                             [target](const std::tuple<int, int, int> &a, const std::tuple<int, int, int> &b) {
                                 int aVal = std::get<0>(a) * std::get<2>(a) + std::get<1>(a);
                                 int bVal = std::get<0>(b) * std::get<2>(b) + std::get<1>(b);
                                 return std::abs(aVal - target) < std::abs(bVal - target);
                             });
}

//------------------------------------------------------------------------
template <typename T> T PerTok::getClosestArrayValue(T value, const std::vector<T> &array) const
{
    auto minIt = std::min_element(array.begin(), array.end(), [value](const T &a, const T &b) {
        return std::abs(a - value) < std::abs(b - value);
    });
    return *minIt;
}

//------------------------------------------------------------------------
int PerTok::findClosestPositionTok(int target) const
{
    return getClosestArrayValue(target, positionLocations);
}

//------------------------------------------------------------------------
Event PerTok::createDurationEvent(const NoteType &note, int program,
                                  const std::vector<std::pair<int, int>> &ticksPerBeat, int tpbIdx) const
{
    const std::tuple<int, int, int> duration = getClosestDurationTuple(note.duration);
    const std::string durString              = libmusictokUtils::join(duration, ".");

    return Event("Duration", durString, note.start(), program, "duration " + std::to_string(note.duration));
}

//------------------------------------------------------------------------
std::vector<int> PerTok::createMicrotimingTickValues() const
{
    int mtBins = config.additionalParams.at("num_microtiming_bins").get<int>();
    std::vector<double> mtVals =
        libmusictokUtils::linspace(-static_cast<double>(maxMtShift), static_cast<double>(maxMtShift), mtBins + 1);

    std::vector<int> out;
    out.reserve(mtBins + 1);

    for (size_t i = 0; i < mtVals.size(); i++)
    {
        out.push_back(static_cast<int>(std::round(mtVals.at(i))));
    }

    return out;
}

//------------------------------------------------------------------------
std::vector<int> PerTok::createTimeshiftTickValues() const
{
    std::set<int> tickValuesSet = {0};
    for (const auto &[beat, subdiv, resolution] : durations)
    {
        int tickValue = static_cast<int>((beat + static_cast<double>(subdiv) / resolution) * tpq);
        tickValuesSet.insert(tickValue);
    }

    return std::vector<int>(tickValuesSet.begin(), tickValuesSet.end());
}

//------------------------------------------------------------------------
std::vector<int> PerTok::createPositionTokLocations() const
{
    std::set<int> positionValues = {0};
    for (const auto &[beat, subdiv, resolution] : durations)
    {
        positionValues.insert((beat * tpq) + subdiv);
    }

    return std::vector<int>(positionValues.begin(), positionValues.end());
}

//------------------------------------------------------------------------
std::vector<std::tuple<int, int, int>> PerTok::createDurationTuples()
{
    std::vector<std::tuple<int, int, int>> outDurations;
    std::set<int> timeShift;
    // Iterate through each beat range and its associated beat resolution
    for (const auto &[beatRange, resolution] : config.beatRes)
    {
        int beatStart = beatRange.first;
        int beatEnd   = beatRange.second;
        for (int beat = beatStart; beat < beatEnd; ++beat)
        {
            for (int subdiv = 0; subdiv < resolution; ++subdiv)
            {
                if (!(beat == 0 && subdiv == 0))
                {
                    int subres = subdiv != 0 ? (tpq / resolution * subdiv) : 0;
                    outDurations.emplace_back(beat, subres, tpq);
                    timeShift.insert((beat * tpq + subres));
                }
            }
        }
    }

    // the first element in the set is the lowest.
    minTimeShift = *timeShift.begin() * 0.5;
    return outDurations;
}

//------------------------------------------------------------------------
int PerTok::convertDurationsToTicks(const std::string &durationStr)
{
    const auto &[beats, subdiv, tpq] = libmusictokUtils::parseDuration(durationStr);
    return beats * tpq + subdiv;
}

} // namespace libmusictok
