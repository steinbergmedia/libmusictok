//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/utility_functions.cpp
// Created by  : Steinberg, 02/2025
// Description : Provides some utility functions commonly used in the
//      libmusictok implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/utility_functions.h"
#include <codecvt>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace libmusictokUtils {

//------------------------------------------------------------------------
// Definition of loadJson
nlohmann::json loadJson(const std::filesystem::path &jsonFilePath)
{
    nlohmann::json jsonConfig;
    std::ifstream infile(jsonFilePath);
    if (!infile)
        throw std::runtime_error("Unable to open file: " + jsonFilePath.string());
    infile >> jsonConfig;
    return jsonConfig;
}

//------------------------------------------------------------------------
nlohmann::ordered_json loadOrderedJson(const std::filesystem::path &jsonFilePath)
{
    nlohmann::ordered_json jsonConfig;
    std::ifstream infile(jsonFilePath);
    if (!infile)
        throw std::runtime_error("Unable to open file: " + jsonFilePath.string());
    infile >> jsonConfig;
    return jsonConfig;
}

//------------------------------------------------------------------------
// Helper: splits a string at the given delimiter.
std::vector<std::string> split(const std::string &s, const std::string &delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end   = s.find(delimiter);

    while (end != std::string::npos)
    {
        tokens.push_back(s.substr(start, end - start));
        start = end + delimiter.length();
        end   = s.find(delimiter, start);
    }
    tokens.push_back(s.substr(start)); // Add the last token

    return tokens;
}

//------------------------------------------------------------------------
std::tuple<int, int, int> parseDuration(const std::string &durationStr)
{
    std::vector<std::string> parts = split(durationStr, ".");
    if (parts.size() != 3)
    {
        throw std::invalid_argument("tried to parse \"" + durationStr + "\" as a duration.");
    }
    int bar, beat, tick;
    try
    {
        bar  = std::stoi(parts[0]);
        beat = std::stoi(parts[1]);
        tick = std::stoi(parts[2]);
    }
    catch (const std::exception &)
    {
        throw std::invalid_argument("tried to parse \"" + durationStr + "\" as a duration.");
    }
    return {bar, beat, tick};
}

//------------------------------------------------------------------------
std::string join(const std::vector<std::string> &parts, const std::string &delimiter)
{
    if (parts.empty())
    {
        return "";
    }
    std::ostringstream oss;
    oss << parts[0];

    for (size_t i = 1; i < parts.size(); i++)
    {
        oss << delimiter << parts[i];
    }
    return oss.str();
}

//------------------------------------------------------------------------
std::string join(const std::tuple<int, int, int> &parts, const std::string &delimiter)
{
    std::ostringstream oss;
    oss << std::to_string(std::get<0>(parts));
    oss << delimiter << std::to_string(std::get<1>(parts));
    oss << delimiter << std::to_string(std::get<2>(parts));

    return oss.str();
}

//------------------------------------------------------------------------
std::u16string utf8_to_utf16(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.from_bytes(str);
}

//------------------------------------------------------------------------
std::string utf32_to_utf8(const std::u16string &utf16_str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(utf16_str);
}

//------------------------------------------------------------------------
std::u32string utf8_to_utf32(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.from_bytes(str);
}

//------------------------------------------------------------------------
std::string utf32_to_utf8(const std::u32string &utf32_str)
{
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.to_bytes(utf32_str);
}

//------------------------------------------------------------------------
libmusictok::ScoreType loadScoreFromMidi(const std::filesystem::path &scoreFile)
{
    std::ifstream inFile(scoreFile, std::ios::binary);
    std::vector<uint8_t> readData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    const std::string fileExtension = scoreFile.extension().string();
    if (fileExtension == ".mid" || fileExtension == ".midi")
    {
        auto score = libmusictok::ScoreType::parse<symusic::DataFormat::MIDI>(std::span<const uint8_t>(readData));
        return score;
    }
    throw std::runtime_error("Unsupported file format: " + fileExtension);
}

//------------------------------------------------------------------------
void saveMidiFromScore(const libmusictok::ScoreType &score, const std::filesystem::path &outputPath)
{
    std::vector<uint8_t> midiData = score.dumps<symusic::DataFormat::MIDI>();
    std::ofstream file(outputPath, std::ios::binary);
    file.write(reinterpret_cast<const char *>(midiData.data()), midiData.size());
    file.close();
}

//------------------------------------------------------------------------
void mergeSameProgramTracks(symusic::shared<symusic::vec<symusic::shared<libmusictok::TrackType>>> &tracks,
                            bool effects)
{
    int n = static_cast<int>(tracks->size());
    if (n == 0)
        return;

    // Gather program numbers for each track, drums get -1
    std::vector<int> tracksPrograms(n, -1);
    for (int i = 0; i < n; ++i)
    {
        if (!(*tracks)[i]->is_drum)
            tracksPrograms[i] = static_cast<int>((*tracks)[i]->program);
        else
            tracksPrograms[i] = -1; // Mark drums with -1
    }

    // Map from program to indices
    std::map<int, std::vector<int>> progToIndices;
    for (int i = 0; i < n; ++i)
        progToIndices[tracksPrograms[i]].push_back(i);

    // Gather groups that are duplicated
    std::vector<std::vector<int>> idxGroups;
    for (const auto &kv : progToIndices)
    {
        const auto &group = kv.second;
        if (group.size() >= 2)
            idxGroups.push_back(group);
    }

    if (idxGroups.empty())
        return;

    // Merge tracks and mark which indices will be deleted
    std::vector<int> idxToDelete;
    for (const auto &idxGroup : idxGroups)
    {
        // Gather all tracks to merge
        std::vector<symusic::shared<libmusictok::TrackType>> to_merge;
        for (int idx : idxGroup)
            to_merge.push_back((*tracks)[idx]);
        // Merge tracks function needs to be implemented appropriately
        auto new_track         = mergeTracks(to_merge, effects);
        (*tracks)[idxGroup[0]] = new_track;
        // Mark all other indices in group for deletion
        for (size_t k = 1; k < idxGroup.size(); ++k)
        {
            idxToDelete.push_back(idxGroup[k]);
        }
    }

    // Sort indices in decreasing order for safe deletion
    std::stable_sort(idxToDelete.begin(), idxToDelete.end(), std::greater<int>());
    for (int idx : idxToDelete)
    {
        tracks->erase(tracks->begin() + idx);
    }
}

//------------------------------------------------------------------------
symusic::shared<libmusictok::TrackType> mergeTracks(
    const symusic::vec<symusic::shared<libmusictok::TrackType>> &tracksToMerge, bool effects)
{
    // Use the first track as the starting point.
    libmusictok::TrackType mergedTrack = *(tracksToMerge[0]);
    std::string mergedName             = mergedTrack.name;

    // Create local arrays for events.
    std::vector<libmusictok::NoteType> mergedNotes(tracksToMerge[0]->notes->begin(), tracksToMerge[0]->notes->end());

    std::vector<libmusictok::PedalType> mergedPedals;
    std::vector<libmusictok::PitchBendType> mergedPitchBends;
    std::vector<libmusictok::ControlChangeType> mergedControls;

    if (effects)
    {
        mergedPedals.assign(tracksToMerge[0]->pedals->begin(), tracksToMerge[0]->pedals->end());
        mergedPitchBends.assign(tracksToMerge[0]->pitch_bends->begin(), tracksToMerge[0]->pitch_bends->end());
        mergedControls.assign(tracksToMerge[0]->controls->begin(), tracksToMerge[0]->controls->end());
    }

    // Process the remaining tracks.
    for (size_t i = 1; i < tracksToMerge.size(); ++i)
    {
        auto track = tracksToMerge[i];

        // Merge track names.
        mergedName += " / " + track->name;

        // Merge notes.
        mergedNotes.insert(mergedNotes.end(), track->notes->begin(), track->notes->end());

        if (effects)
        {
            mergedPedals.insert(mergedPedals.end(), track->pedals->begin(), track->pedals->end());
            mergedPitchBends.insert(mergedPitchBends.end(), track->pitch_bends->begin(), track->pitch_bends->end());
            mergedControls.insert(mergedControls.end(), track->controls->begin(), track->controls->end());
        }
    }

    // Sort each collection by time.
    std::stable_sort(mergedNotes.begin(), mergedNotes.end(),
                     [](const libmusictok::NoteType &a, const libmusictok::NoteType &b) { return a.time < b.time; });

    if (effects)
    {
        std::stable_sort(
            mergedPedals.begin(), mergedPedals.end(),
            [](const libmusictok::PedalType &a, const libmusictok::PedalType &b) { return a.time < b.time; });

        std::stable_sort(
            mergedPitchBends.begin(), mergedPitchBends.end(),
            [](const libmusictok::PitchBendType &a, const libmusictok::PitchBendType &b) { return a.time < b.time; });

        // Sort controls by time, then number, then value - not explicitely specified in MIDItok but it does it automatically...
        std::stable_sort(mergedControls.begin(), mergedControls.end(),
                         [](const libmusictok::ControlChangeType &a, const libmusictok::ControlChangeType &b) {
                             if (a.time != b.time)
                                 return a.time < b.time;
                             if (a.number != b.number)
                                 return a.number < b.number;
                             return a.value < b.value;
                         });
    }

    // Create new pyvec objects from the sorted std::vector arrays.
    auto notesPyvec = std::make_shared<symusic::pyvec<libmusictok::NoteType>>();
    for (const auto &note : mergedNotes)
    {
        notesPyvec->append(note);
    }
    mergedTrack.notes = notesPyvec;

    if (effects)
    {
        auto pedalsPyvec     = std::make_shared<symusic::pyvec<libmusictok::PedalType>>();
        auto pitchBendsPyvec = std::make_shared<symusic::pyvec<libmusictok::PitchBendType>>();
        auto controlsPyvec   = std::make_shared<symusic::pyvec<libmusictok::ControlChangeType>>();

        for (const auto &pedal : mergedPedals)
        {
            pedalsPyvec->append(pedal);
        }
        for (const auto &pitchBend : mergedPitchBends)
        {
            pitchBendsPyvec->append(pitchBend);
        }
        for (const auto &control : mergedControls)
        {
            controlsPyvec->append(control);
        }

        mergedTrack.pedals      = pedalsPyvec;
        mergedTrack.pitch_bends = pitchBendsPyvec;
        mergedTrack.controls    = controlsPyvec;
    }

    // Set the merged track name.
    mergedTrack.name = mergedName;

    return std::make_shared<libmusictok::TrackType>(mergedTrack);
}

//------------------------------------------------------------------------
bool areTimeSignaturesEqual(const libmusictok::TimeSigType &ts1, const libmusictok::TimeSigType &ts2)
{
    return (ts1->numerator == ts2->numerator) && (ts1->denominator == ts2->denominator);
}

//------------------------------------------------------------------------
int computeTicksPerBeat(int timeSignatureDenominator, int timeDiv)
{
    if (timeSignatureDenominator == 4)
    {
        return timeDiv;
    }

    double timeDivFactor = 4 / timeSignatureDenominator;
    return static_cast<int>(timeDiv * timeDivFactor);
}

//------------------------------------------------------------------------
int computeTicksPerBar(const libmusictok::TimeSigType &timeSig, int timeDiv)
{
    return computeTicksPerBeat(timeSig->denominator, timeDiv) * timeSig->numerator;
}

//------------------------------------------------------------------------
int computeTicksPerBar(const std::pair<int, int> &timeSig, int timeDiv)
{
    return computeTicksPerBeat(timeSig.second, timeDiv) * timeSig.first;
}

//------------------------------------------------------------------------
std::vector<std::pair<int, int>> getScoreTicksPerBeat(const libmusictok::ScoreType &score)
{
    auto &tsigs = *score.time_signatures;
    std::vector<std::pair<int, int>> ticksPerBeat;

    const int n_ts = static_cast<int>(tsigs.size());

    if (n_ts == 0)
    {
        return ticksPerBeat;
    }

    // For each time signature change except the last,
    // record [ending tick, ticksPerBeat for this section]
    for (int tsi = 0; tsi < n_ts - 1; ++tsi)
    {
        const auto &tsiCurrent = tsigs[tsi];
        const auto &tsiNext    = tsigs[tsi + 1];
        int endTick            = int(tsiNext.time);
        int tpBeat             = computeTicksPerBeat(tsiCurrent.denominator, score.ticks_per_quarter);
        ticksPerBeat.push_back({endTick, tpBeat});
    }

    // Handle last time signature up to score.end() + 1
    int lastTpBeat = computeTicksPerBeat(tsigs.back().denominator, score.ticks_per_quarter);
    // Score::end() returns the max tick for the full Score
    int lastEnd = int(score.end()) + 1;
    ticksPerBeat.push_back({lastEnd, lastTpBeat});

    // Remove equal successive ones
    for (int i = static_cast<int>(ticksPerBeat.size()) - 1; i > 0; --i)
    {
        if (ticksPerBeat[i].second == ticksPerBeat[i - 1].second)
        {
            ticksPerBeat[i - 1].first = ticksPerBeat[i].first;
            ticksPerBeat.erase(ticksPerBeat.begin() + i);
        }
    }

    return ticksPerBeat;
}

//------------------------------------------------------------------------
// vector i32 version
std::vector<symusic::i32> npGetClosest(const std::vector<symusic::i32> &referenceArray,
                                       const std::vector<symusic::i32> &valueArray)
{
    std::vector<symusic::i32> result;
    result.reserve(valueArray.size());

    for (symusic::i32 value : valueArray)
    {
        result.push_back(npGetClosest(referenceArray, value));
    }

    return result;
}

//------------------------------------------------------------------------
// vector i8 version
std::vector<symusic::i8> npGetClosest(const std::vector<symusic::i8> &referenceArray,
                                      const std::vector<symusic::i8> &valueArray)
{
    std::vector<symusic::i8> result;
    result.reserve(valueArray.size());

    for (symusic::i8 value : valueArray)
    {
        result.push_back(npGetClosest(referenceArray, value));
    }

    return result;
}

//------------------------------------------------------------------------
// vector double version
std::vector<double> npGetClosest(const std::vector<double> &referenceArray, const std::vector<double> &valueArray)
{
    std::vector<double> result;
    result.reserve(valueArray.size());

    for (double value : valueArray)
    {
        result.push_back(npGetClosest(referenceArray, value));
    }

    return result;
}

//------------------------------------------------------------------------
// single i32 version
symusic::i32 npGetClosest(const std::vector<symusic::i32> &referenceArray, const symusic::i32 &value)
{
    if (referenceArray.empty())
        return 0;

    // Binary search for insertion position
    auto it    = std::lower_bound(referenceArray.begin(), referenceArray.end(), value);
    size_t idx = static_cast<size_t>(it - referenceArray.begin());

    size_t leftIdx  = (idx == 0) ? 0 : idx - 1;
    size_t rightIdx = (idx >= referenceArray.size()) ? referenceArray.size() - 1 : idx;

    auto leftDist  = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[leftIdx]));
    auto rightDist = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[rightIdx]));

    size_t bestIdx = (rightDist <= leftDist) ? rightIdx : leftIdx;
    return referenceArray[bestIdx];
}

//------------------------------------------------------------------------
// single i8 version
symusic::i8 npGetClosest(const std::vector<symusic::i8> &referenceArray, const symusic::i8 &value)
{
    if (referenceArray.empty())
        return 0;

    // Binary search for insertion position
    auto it    = std::lower_bound(referenceArray.begin(), referenceArray.end(), value);
    size_t idx = static_cast<size_t>(it - referenceArray.begin());

    size_t leftIdx  = (idx == 0) ? 0 : idx - 1;
    size_t rightIdx = (idx >= referenceArray.size()) ? referenceArray.size() - 1 : idx;

    auto leftDist  = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[leftIdx]));
    auto rightDist = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[rightIdx]));

    size_t bestIdx = (rightDist <= leftDist) ? rightIdx : leftIdx;
    return referenceArray[bestIdx];
}

//------------------------------------------------------------------------
// single double version
double npGetClosest(const std::vector<double> &referenceArray, const double &value)
{
    if (referenceArray.empty())
        return 0;

    // Binary search for insertion position
    auto it    = std::lower_bound(referenceArray.begin(), referenceArray.end(), value);
    size_t idx = static_cast<size_t>(it - referenceArray.begin());

    size_t leftIdx  = (idx == 0) ? 0 : idx - 1;
    size_t rightIdx = (idx >= referenceArray.size()) ? referenceArray.size() - 1 : idx;

    auto leftDist  = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[leftIdx]));
    auto rightDist = std::abs(static_cast<int>(value) - static_cast<int>(referenceArray[rightIdx]));

    size_t bestIdx = (rightDist <= leftDist) ? rightIdx : leftIdx;
    return referenceArray[bestIdx];
}

//------------------------------------------------------------------------
void removeDuplicatedNotes(symusic::pyvec<libmusictok::NoteType> &notes, bool considerDuration)
{
    if (notes.size() <= 1)
    {
        return;
    }

    // Find repeated notes (having same time, pitch, [duration])
    std::vector<size_t> idxToRemove;

    for (size_t i = 1; i < notes.size(); ++i)
    {
        bool same = notes[i].time == notes[i - 1].time && notes[i].pitch == notes[i - 1].pitch;
        if (considerDuration)
        {
            same = same && (notes[i].duration == notes[i - 1].duration);
        }

        if (same)
        {
            idxToRemove.push_back(i);
        }
    }

    // Remove in reverse order to keep indices valid
    for (auto it = idxToRemove.rbegin(); it != idxToRemove.rend(); ++it)
    {
        notes.erase(notes.begin() + *it);
    }
}

//------------------------------------------------------------------------
bool isTrackEmpty(const libmusictok::TrackType &track, bool checkControls, bool checkPedals, bool checkPitchBends)
{
    if (checkControls && checkPedals && checkPitchBends)
    {
        return track.empty();
    }

    bool isEmpty = (track.note_num() == 0);

    if (checkControls)
    {
        isEmpty = (isEmpty && track.controls->empty());
    }

    if (checkPedals)
    {
        isEmpty = (isEmpty && track.pedals->empty());
    }

    if (checkPitchBends)
    {
        isEmpty = (isEmpty && track.pitch_bends->empty());
    }

    return isEmpty;
}

//------------------------------------------------------------------------
std::vector<double> tempoQpmToMspq(const std::vector<double> &tempoQpm)
{
    std::vector<double> out;
    out.reserve(tempoQpm.size());

    for (double qpm : tempoQpm)
    {
        out.push_back(60000000. / qpm);
    }
    return out;
}

//------------------------------------------------------------------------
template <typename T> std::vector<size_t> histogram(const std::vector<T> &data, const std::vector<T> &bins)
{
    std::vector<size_t> counts(bins.size() - 1, 0);

    for (const auto &x : data)
    {
        // Find correct bin for x
        // Bins are assumed to be monotonically increasing.
        // numpy behavior: rightmost bin is inclusive, others: left inclusive, right exclusive
        auto it    = std::upper_bound(bins.begin(), bins.end(), x);
        size_t idx = std::distance(bins.begin(), it) - 1;

        // idx < 0 means x < bins[0], ignore
        // idx >= counts.size() means x >= bins.back()
        if (idx < counts.size() && idx >= 0)
        {
            // Special case: include last bin right edge
            if (idx == counts.size() - 1 && x == bins.back())
            {
                counts[idx]++;
            }
            else if (idx < counts.size())
            {
                if (x >= bins[idx] && (x < bins[idx + 1]))
                {
                    counts[idx]++;
                }
            }
        }
    }

    return counts;
}

//------------------------------------------------------------------------
// https://stackoverflow.com/questions/27028226/python-linspace-in-c
template <typename T> std::vector<double> linspace(T startIn, T endIn, int numIn)
{
    std::vector<double> linspaced;

    double start = static_cast<double>(startIn);
    double end   = static_cast<double>(endIn);
    double num   = static_cast<double>(numIn);

    if (num == 0)
    {
        return linspaced;
    }
    if (num == 1)
    {
        linspaced.push_back(start);
        return linspaced;
    }

    double delta = (end - start) / (num - 1);

    for (int i = 0; i < num - 1; ++i)
    {
        linspaced.push_back(start + delta * i);
    }
    linspaced.push_back(end);
    return linspaced;
}

//------------------------------------------------------------------------
// Overload for a single TokSequence
void addBarBeatsTicksToTokseq(libmusictok::TokSequence &tokseq, libmusictok::ScoreType &score)
{
    std::vector<int> barTicks  = getBarsTicks(score, true);
    std::vector<int> beatTicks = getBeatsTicks(score, true);
    tokseq.setTicksBars(barTicks);
    tokseq.setTicksBeats(beatTicks);
}

//------------------------------------------------------------------------
void addBarBeatsTicksToTokseq(libmusictok::TokSequence &tokseq, std::vector<int> &barTicks, std::vector<int> &beatTicks)
{
    tokseq.setTicksBars(barTicks);
    tokseq.setTicksBeats(beatTicks);
}

//------------------------------------------------------------------------
// Overload for a vector of TokSequences
void addBarBeatsTicksToTokseq(std::vector<libmusictok::TokSequence> &tokseqs, libmusictok::ScoreType &score)
{
    std::vector<int> barTicks  = getBarsTicks(score, true);
    std::vector<int> beatTicks = getBeatsTicks(score, true);

    for (auto &seq : tokseqs)
    {
        addBarBeatsTicksToTokseq(seq, barTicks, beatTicks);
    }
}

//------------------------------------------------------------------------
void addBarBeatsTicksToTokseq(std::vector<libmusictok::TokSequence> &tokseqs, std::vector<int> &barTicks,
                              std::vector<int> &beatTicks)
{
    for (auto &seq : tokseqs)
    {
        addBarBeatsTicksToTokseq(seq, barTicks, beatTicks);
    }
}

//------------------------------------------------------------------------
libmusictok::ScoreType::unit getMaxTickOnlyOnsets(const libmusictok::ScoreType &score)
{
    using unit = typename libmusictok::ScoreType::unit;
    std::vector<unit> maxTickTracks;

    for (const auto &trackPtr : *score.tracks)
    {
        const auto &track = *trackPtr;
        unit maxNote      = track.notes->empty() ? 0 : track.notes->back().time;
        unit maxControl   = track.controls->empty() ? 0 : track.controls->back().time;
        unit maxPitch     = track.pitch_bends->empty() ? 0 : track.pitch_bends->back().time;
        unit maxLyric     = track.lyrics->empty() ? 0 : track.lyrics->back().time;
        maxTickTracks.push_back(std::max({maxNote, maxControl, maxPitch, maxLyric}));
    }

    unit maxTempo   = score.tempos->empty() ? 0 : score.tempos->back().time;
    unit maxTimesig = score.time_signatures->empty() ? 0 : score.time_signatures->back().time;
    unit maxKeysig  = score.key_signatures->empty() ? 0 : score.key_signatures->back().time;
    unit maxMarker  = score.markers->empty() ? 0 : score.markers->back().time;

    unit maxTracks = maxTickTracks.empty() ? 0 : *std::max_element(maxTickTracks.begin(), maxTickTracks.end());

    return std::max({maxTempo, maxTimesig, maxKeysig, maxMarker, maxTracks});
}

//------------------------------------------------------------------------
std::vector<int> getBarsTicks(const libmusictok::ScoreType &score, bool onlyNotesOnsets)
{
    libmusictok::ScoreType::unit maxTickUnit = onlyNotesOnsets ? getMaxTickOnlyOnsets(score) : score.end();
    int maxTick                              = static_cast<int>(maxTickUnit);

    if (maxTick == 0)
    {
        return {0};
    }

    std::vector<int> barsTicks;
    symusic::pyvec<libmusictok::TimeSigType> timeSigs = *score.time_signatures;

    if (timeSigs.size() == 0)
    {
        timeSigs.append(libmusictok::TimeSigType(0, libmusictok::timeSignature_default.first,
                                                 libmusictok::timeSignature_default.second));
    }
    // Mock the last one to cover the last section in the loop below in all cases, as it
    // prevents the cas in which the last time signature has an invalid numerator or
    // denominator (that would have been skipped in the while loop below
    timeSigs.append(libmusictok::TimeSigType(maxTickUnit, libmusictok::timeSignature_default.first,
                                             libmusictok::timeSignature_default.second));

    // Section from tick 0 to first time sig is 4/4 if first time sig time not 0
    libmusictok::TimeSigType currentTimeSig;
    if (timeSigs[0].time == 0 && timeSigs[0].denominator > 0 && timeSigs[0].numerator > 0)
    {
        currentTimeSig = timeSigs[0];
    }
    else
    {
        currentTimeSig = libmusictok::TimeSigType(0, libmusictok::timeSignature_default.first,
                                                  libmusictok::timeSignature_default.second);
    }

    // compute bars, one time signature portion at a time.
    for (size_t i = 1; i < timeSigs.size(); ++i)
    {
        if (timeSigs[i].denominator <= 0 || timeSigs[i].numerator <= 0)
        {
            continue;
        }

        int ticksPerBar                        = computeTicksPerBar(currentTimeSig, score.ticks_per_quarter);
        libmusictok::ScoreType::unit ticksDiff = timeSigs[i].time - currentTimeSig.time;
        int numBars                            = std::ceil((double)ticksDiff / (double)ticksPerBar);

        for (int j = 0; j < numBars; ++j)
        {
            barsTicks.push_back(static_cast<int>(currentTimeSig.time) + (ticksPerBar * j));
        }
    }

    return barsTicks;
}

//------------------------------------------------------------------------
std::vector<int> getBeatsTicks(const libmusictok::ScoreType &score, bool onlyNotesOnsets)
{
    libmusictok::ScoreType::unit maxTickUnit = onlyNotesOnsets ? getMaxTickOnlyOnsets(score) : score.end();
    int maxTick                              = static_cast<int>(maxTickUnit);

    if (maxTick == 0)
    {
        return {0};
    }

    std::vector<int> beatsTicks;
    symusic::pyvec<libmusictok::TimeSigType> timeSigs = *score.time_signatures;

    if (timeSigs.size() == 0)
    {
        timeSigs.append(libmusictok::TimeSigType(0, libmusictok::timeSignature_default.first,
                                                 libmusictok::timeSignature_default.second));
    }
    // Mock the last one to cover the last section in the loop below in all cases, as it
    // prevents the cas in which the last time signature has an invalid numerator or
    // denominator (that would have been skipped in the while loop below
    timeSigs.append(libmusictok::TimeSigType(maxTickUnit, libmusictok::timeSignature_default.first,
                                             libmusictok::timeSignature_default.second));

    // Section from tick 0 to first time sig is 4/4 if first time sig time not 0
    libmusictok::TimeSigType currentTimeSig;
    if (timeSigs[0].time == 0 && timeSigs[0].denominator > 0 && timeSigs[0].numerator > 0)
    {
        currentTimeSig = timeSigs[0];
    }
    else
    {
        currentTimeSig = libmusictok::TimeSigType(0, libmusictok::timeSignature_default.first,
                                                  libmusictok::timeSignature_default.second);
    }

    // compute bars, one time signature portion at a time.
    for (size_t i = 1; i < timeSigs.size(); ++i)
    {
        if (timeSigs[i].denominator <= 0 || timeSigs[i].numerator <= 0)
        {
            continue;
        }

        int ticksPerBeat = computeTicksPerBeat(currentTimeSig.denominator, score.ticks_per_quarter);
        libmusictok::ScoreType::unit ticksDiff = timeSigs[i].time - currentTimeSig.time;
        int numBeats                           = std::ceil((double)ticksDiff / (double)ticksPerBeat);

        for (int j = 0; j < numBeats; ++j)
        {
            beatsTicks.push_back(static_cast<int>(currentTimeSig.time) + (ticksPerBeat * j));
        }
    }

    return beatsTicks;
}

//------------------------------------------------------------------------
std::vector<libmusictok::Event> detectChords(symusic::shared<symusic::pyvec<libmusictok::NoteType>> notes,
                                             const std::vector<std::pair<int, int>> &ticksPerBeat,
                                             const std::vector<std::pair<std::string, std::vector<int>>> &chordMaps,
                                             int program, bool specifyRootNote, int beatRes, int onsetOffset,
                                             std::vector<int> unknownChordsNumNotesRange, int simulNotesLimit)
{
    if (simulNotesLimit < 5)
    {
        simulNotesLimit = 5;
        std::clog << "`simulNotesLimit` must be >= 5, as chords can be made up of up to 5 notes. Setting it to 5."
                  << std::endl;
    }

    // Prepare note triplets: (pitch, start, end)
    std::vector<std::tuple<int, int, int>> notesVec(notes->size());
    for (size_t i = 0; i < notes->size(); ++i)
    {
        auto &note  = (*notes)[i];
        notesVec[i] = {static_cast<int>(note.pitch), static_cast<int>(note.start()), static_cast<int>(note.end())};
    }

    int tpbIndex        = 0;
    int tpbHalf         = ticksPerBeat[tpbIndex].second / 2;
    int onsetOffsetTick = static_cast<int>(double(ticksPerBeat[tpbIndex].second * onsetOffset) / double(beatRes));

    int count        = 0;
    int previousTick = -1;
    std::vector<libmusictok::Event> chords;

    while (count < int(notesVec.size()))
    {
        // Checks we moved in time after last step, otherwise discard this tick
        int thisNoteStart = std::get<1>(notesVec[count]);
        if (thisNoteStart == previousTick)
        {
            ++count;
            continue;
        }

        // Update the time offset the time signature denom/ticks per beat changed
        // `while` as there might not be any note in the next section
        while (tpbIndex + 1 < int(ticksPerBeat.size()) && thisNoteStart >= ticksPerBeat[tpbIndex + 1].first)
        {
            ++tpbIndex;
            tpbHalf         = ticksPerBeat[tpbIndex].second / 2;
            onsetOffsetTick = static_cast<int>(double(ticksPerBeat[tpbIndex].second * onsetOffset) / double(beatRes));
        }

        // Gather possibly simultaneous notes (up to simulNotesLimit, all with start within onsetOffsetTick)
        std::vector<std::tuple<int, int, int>> onsetNotes;
        onsetNotes.reserve(simulNotesLimit);
        int onsetStart = std::get<1>(notesVec[count]);
        for (int j = count; j < std::min(int(notesVec.size()), count + simulNotesLimit); ++j)
        {
            if (std::get<1>(notesVec[j]) <= onsetStart + onsetOffsetTick)
            {
                onsetNotes.push_back(notesVec[j]);
            }
            else
            {
                break;
            }
        }
        if (onsetNotes.empty())
        {
            ++count;
            continue;
        }

        // If ambiguous (note lengths too different), skip to next
        int onsetEnd0  = std::get<2>(onsetNotes[0]);
        bool ambiguous = false;
        for (auto &nt : onsetNotes)
        {
            if (std::abs(std::get<2>(nt) - onsetEnd0) > tpbHalf)
            {
                ambiguous = true;
            }
        }
        if (ambiguous)
        {
            count += onsetNotes.size();
            continue;
        }

        // If notes are short, keep only those exactly on onset
        if ((std::get<2>(notesVec[count]) - std::get<1>(notesVec[count])) <= tpbHalf)
        {
            std::vector<std::tuple<int, int, int>> filtered;
            for (auto &nt : onsetNotes)
            {
                if (std::get<1>(nt) == onsetStart)
                {
                    filtered.push_back(nt);
                }
            }
            onsetNotes = std::move(filtered);
        }

        // Keep only notes whose ends are within tpbHalf from first note
        std::vector<std::tuple<int, int, int>> chordNotes;
        for (auto &nt : onsetNotes)
        {
            if (std::abs(std::get<2>(nt) - onsetEnd0) <= tpbHalf)
            {
                chordNotes.push_back(nt);
            }
        }

        // Construct chord map (intervals from root)
        if (chordNotes.size() >= 3 && chordNotes.size() <= 5)
        {
            // Chord interval map relative to lowest pitch
            std::vector<int> chordMap;
            int rootPitch = std::get<0>(chordNotes[0]);
            for (const auto &nt : chordNotes)
            {
                chordMap.push_back(std::get<0>(nt) - rootPitch);
            }

            // Only analyze if highest interval is reasonably close (within 24 semitones)
            if (!chordMap.empty() && chordMap.back() <= 24)
            {
                std::string chordQuality =
                    libmusictok::unknownChordPrefix_default + std::to_string(int(chordNotes.size()));
                bool isUnknownChord = true;
                for (const auto &[quality, knownChord] : chordMaps)
                {
                    if (knownChord == chordMap)
                    {
                        chordQuality   = quality;
                        isUnknownChord = false;
                        break;
                    }
                }
                // If found, or want to label unknowns, add event
                if (!unknownChordsNumNotesRange.empty() || !isUnknownChord)
                {
                    if (specifyRootNote)
                    {
                        chordQuality = libmusictok::pitchClasses_default[rootPitch % 12] + ":" + chordQuality;
                    }
                    std::string chordMapStr = "[";
                    for (size_t k = 0; k < chordMap.size(); ++k)
                    {
                        if (k > 0)
                            chordMapStr += ",";
                        chordMapStr += std::to_string(chordMap[k]);
                    }
                    chordMapStr += "]";
                    chords.push_back(
                        libmusictok::Event("Chord", chordQuality,
                                           std::get<1>(*std::min_element(chordNotes.begin(), chordNotes.end(),
                                                                         [](const auto &a, const auto &b) {
                                                                             return std::get<1>(a) < std::get<1>(b);
                                                                         })),
                                           program != -1 ? program : 0, chordMapStr));
                }
            }
        }

        previousTick = std::get<1>(onsetNotes.back());
        count += onsetNotes.size();
    }
    return chords;
}

//------------------------------------------------------------------------
std::string roundToTwoDecimals(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string result = oss.str();
    // Remove trailing zero if last decimal is zero: e.g. 40.00 -> 40.0
    if (result.substr(result.size() - 2) == "00")
    {
        result = result.substr(0, result.size() - 1); // remove last '0'
    }
    else if (result[result.size() - 1] == '0')
    {
        result = result.substr(0, result.size() - 1); // remove last '0'
    }
    // Remove .0 if integer, leave one decimal point if not requested
    if (result.substr(result.size() - 2) == ".0")
    {
        return result;
    }
    return result;
}

//------------------------------------------------------------------------
void fixOffsetsOverlappingNotes(symusic::pyvec<libmusictok::NoteType> &notes)
{
    if (notes.size() == 0)
        return;

    for (size_t i = 0; i < notes.size() - 1; ++i)
    {
        size_t j = i + 1;
        while (j < notes.size() && notes[j].start() <= notes[i].end())
        {
            if (notes[i].pitch == notes[j].pitch)
            {
                notes[i].duration = notes[j].start() - notes[i].start();
                // breaks as no other notes with start before this one
                break;
            }
            ++j;
        }
    }
}

//------------------------------------------------------------------------
double bankersRound(double value)
{
    double intPart;
    double fracPart = std::modf(value, &intPart);

    if (std::abs(fracPart) == 0.5)
    {
        // Round to nearest even
        return (static_cast<int>(intPart) % 2 == 0) ? intPart : intPart + std::copysign(1.0, value);
    }
    return std::round(value);
}

//------------------------------------------------------------------------
std::vector<std::string> removeDuplicatesPreserveOrder(const std::vector<std::string> &input)
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    for (const auto &item : input)
    {
        if (seen.find(item) == seen.end())
        {
            result.push_back(item);
            seen.insert(item);
        }
    }
    return result;
}

//------------------------------------------------------------------------
// explicit instantiations
//------------------------------------------------------------------------
template std::vector<double> libmusictokUtils::linspace<double>(double, double, int);
template std::vector<size_t> libmusictokUtils::histogram<int>(const std::vector<int> &, const std::vector<int> &);

namespace Internal {
//------------------------------------------------------------------------
void checkInst(int prog, std::map<int, libmusictok::TrackType> &tracks)
{
    if (tracks.find(prog) == tracks.end())
    {
        bool isDrum  = prog == -1;
        tracks[prog] = libmusictok::TrackType(isDrum ? "Drums" : libmusictok::MIDI_INSTRUMENTS[prog].name,
                                              isDrum ? 0 : prog, isDrum);
    }
}

//------------------------------------------------------------------------
bool isTrackEmpty(const libmusictok::TrackType &track)
{
    return track.notes->size() == 0 && track.controls->size() == 0 && track.pitch_bends->size() == 0;
}

} // namespace Internal

} // namespace libmusictokUtils
