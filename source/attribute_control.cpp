//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/attribute_control.cpp
// Created by  : Steinberg, 07/2025
// Description : AttributeControl class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/attribute_control.h"
#include <set>

namespace libmusictok {

//------------------------------------------------------------------------
// Constructor
AttributeControl::AttributeControl(std::vector<std::string> tokens_)
    : tokens(std::move(tokens_))
{
}
AttributeControl::~AttributeControl() = default;

//------------------------------------------------------------------------
// 								-----------------
// 								| Bar level ACs |
// 								-----------------
// BarAttributeControl
//------------------------------------------------------------------------
std::vector<Event> BarAttributeControl::compute(const TrackType &track, int timeDivision,
                                                const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
                                                const std::vector<int> &barsIdx)
{
    std::vector<Event> attributeControls;

    auto &notes      = *track.notes;
    auto &controls   = *track.controls;
    auto &pitchBends = *track.pitch_bends;

    // Gather only bar ticks that are not beyond last note
    std::vector<int> barTicksTrack;
    if (notes.empty())
    {
        return attributeControls;
    }
    int lastNoteTick = notes.back().time;
    for (int tick : ticksBars)
    {
        if (tick <= lastNoteTick)
        {
            barTicksTrack.push_back(tick);
        }
    }

    size_t noteStartIdx      = 0;
    size_t controlStartIdx   = 0;
    size_t pitchBendStartIdx = 0;

    for (int barIdx : barsIdx)
    {
        // skip this bar if it is beyond the content of current track
        if (barIdx >= static_cast<int>(barTicksTrack.size()))
        {
            continue;
        }

        // find ending indices for slicing
        std::optional<size_t> noteEndIdx, controlEndIdx, pitchBendEndIdx;
        int barEndTick =
            (barIdx + 1 < (int)barTicksTrack.size()) ? barTicksTrack[barIdx + 1] : -1; // if last bar, use -1 as a flag

        if (barEndTick == -1)
        {
            noteEndIdx      = std::nullopt;
            controlEndIdx   = std::nullopt;
            pitchBendEndIdx = std::nullopt;
        }
        else
        {
            // Find which element is >= barEndTick, starting from *_start_idx
            auto findEndIdx = [barEndTick](const auto &vec, size_t start) -> std::optional<size_t> {
                for (size_t i = start; i < vec.size(); ++i)
                    if (vec[i].time >= barEndTick)
                        return i;
                return std::nullopt;
            };
            noteEndIdx      = findEndIdx(notes, noteStartIdx);
            controlEndIdx   = findEndIdx(controls, controlStartIdx);
            pitchBendEndIdx = findEndIdx(pitchBends, pitchBendStartIdx);
        }

        // Only compute bar attributes if the bar is not empty
        // - If noteEndIdx is nullopt, process up to end (last bar)
        // - Or if noteEndIdx exists and covers more than just (noteStartIdx+1)
        size_t ns = noteStartIdx;
        size_t ne = noteEndIdx.value_or(notes.size());
        if (ne > ns)
        {
            // slice notes/controls/pitchBends for this bar
            auto slice = [](const auto &vec, size_t start, size_t end) {
                using E = typename std::decay_t<decltype(vec)>::value_type;
                return std::vector<E>(vec.begin() + start, vec.begin() + end);
            };
            std::vector<NoteType> notesBar = slice(notes, noteStartIdx, ne);
            std::vector<ControlChangeType> controlsBar =
                slice(controls, controlStartIdx, controlEndIdx.value_or(controls.size()));
            std::vector<PitchBendType> pitchBendsBar =
                slice(pitchBends, pitchBendStartIdx, pitchBendEndIdx.value_or(pitchBends.size()));

            // Check a second time in case it is the last bar
            if (!notesBar.empty())
            {
                std::vector<Event> attributeControlsBar =
                    this->computeOnBar(notesBar, controlsBar, pitchBendsBar, timeDivision);

                // Set event time to this bar's start tick
                for (auto &event : attributeControlsBar)
                    event.time = barTicksTrack[barIdx];

                attributeControls.insert(attributeControls.end(), attributeControlsBar.begin(),
                                         attributeControlsBar.end());
            }
        }

        // Break the loop early if the last note is reached
        if (!noteEndIdx.has_value())
            break;

        // advance indices for the next bar
        noteStartIdx      = noteEndIdx.value();
        controlStartIdx   = controlEndIdx.value_or(controls.size());
        pitchBendStartIdx = pitchBendEndIdx.value_or(pitchBends.size());
    }

    return attributeControls;
}

// BarNoteDensity
//------------------------------------------------------------------------
BarNoteDensity::BarNoteDensity(int densityMax_)
    : densityMax(densityMax_)
{
    for (size_t i = 0; i < densityMax; ++i)
    {
        tokens.push_back("ACBarNoteDensity_" + std::to_string(i));
    }
    tokens.push_back("ACBarNoteDensity_" + std::to_string(densityMax) + "+");
}

//------------------------------------------------------------------------
std::vector<Event> BarNoteDensity::computeOnBar(const std::vector<NoteType> &notesBar,
                                                const std::vector<ControlChangeType> &controlsBar,
                                                const std::vector<PitchBendType> &pitchBendsBar,
                                                const int &timeDivision)
{
    int nNotes = static_cast<int>(notesBar.size());
    if (nNotes >= this->densityMax)
    {
        return {Event("ACBarNoteDensity", std::to_string(nNotes) + "+")};
    }
    return {Event("ACBarNoteDensity", nNotes)};
}

// BarOnsetPolyphony
//------------------------------------------------------------------------
BarOnsetPolyphony::BarOnsetPolyphony(int polyphonyMin_, int polyphonyMax_)
    : polyphonyMin(polyphonyMin_)
    , polyphonyMax(polyphonyMax_)
{
    for (size_t i = polyphonyMin; i < polyphonyMax + 1; ++i)
    {
        tokens.push_back("ACBarOnsetPolyphonyMin_" + std::to_string(i));
        tokens.push_back("ACBarOnsetPolyphonyMax_" + std::to_string(i));
    }
}

//------------------------------------------------------------------------
std::vector<Event> BarOnsetPolyphony::computeOnBar(const std::vector<NoteType> &notesBar,
                                                   const std::vector<ControlChangeType> &controlsBar,
                                                   const std::vector<PitchBendType> &pitchBendsBar,
                                                   const int &timeDivision)
{
    // Get note start times
    std::vector<int> noteTicks;
    for (NoteType note : notesBar)
    {
        noteTicks.push_back(static_cast<int>(note.time));
    }

    // Count polyphony at each onset (tick value)
    std::unordered_map<int, int> onsetCounts;
    for (const int tick : noteTicks)
    {
        onsetCounts[tick]++;
    }

    // If there are no notes, set default values
    int onsetPolyMin = 0, onsetPolyMax = 0;
    if (!onsetCounts.empty())
    {
        onsetPolyMin = std::numeric_limits<int>::max();
        onsetPolyMax = std::numeric_limits<int>::min();
        for (const auto &kv : onsetCounts)
        {
            onsetPolyMin = std::min(onsetPolyMin, kv.second);
            onsetPolyMax = std::max(onsetPolyMax, kv.second);
        }
    }
    // Clamp according to polyphony limits
    int minPoly = std::min(std::max(onsetPolyMin, polyphonyMin), polyphonyMax);
    int maxPoly = std::min(onsetPolyMax, polyphonyMax);
    // Results
    return {Event("ACBarOnsetPolyphonyMin", minPoly), Event("ACBarOnsetPolyphonyMax", maxPoly)};
}

// BarPitchClass
//------------------------------------------------------------------------
BarPitchClass::BarPitchClass()
{
    for (int i = 0; i < 12; ++i)
    {
        tokens.push_back("ACBarPitchClass_" + std::to_string(i));
    }
}

//------------------------------------------------------------------------
std::vector<Event> BarPitchClass::computeOnBar(const std::vector<NoteType> &notesBar,
                                               const std::vector<ControlChangeType> &controlsBar,
                                               const std::vector<PitchBendType> &pitchBendsBar, const int &timeDivision)
{
    std::set<int> uniquePitches;
    for (const auto &note : notesBar)
    {
        uniquePitches.insert(static_cast<int>(note.pitch) % 12);
    }
    std::vector<Event> attributeControls;
    for (int pitch : uniquePitches)
    {
        attributeControls.push_back(Event("ACBarPitchClass", pitch));
    }
    return attributeControls;
}

// BarNoteDuration
//------------------------------------------------------------------------
BarNoteDuration::BarNoteDuration()
{
    for (std::string duration : noteDurations)
    {
        tokens.push_back("ACBarNoteDuration" + duration + "_0");
        tokens.push_back("ACBarNoteDuration" + duration + "_1");
    }
}

//------------------------------------------------------------------------
std::vector<Event> BarNoteDuration::computeOnBar(const std::vector<NoteType> &notesBar,
                                                 const std::vector<ControlChangeType> &controlsBar,
                                                 const std::vector<PitchBendType> &pitchBendsBar,
                                                 const int &timeDivision)
{
    std::set<double> durations;
    std::vector<Event> attributeControls;
    for (const NoteType &note : notesBar)
    {
        durations.insert(static_cast<double>(note.duration));
    }

    for (size_t i = 0; i < factors.size(); ++i)
    {
        if (durations.contains(static_cast<double>(timeDivision) * factors[i]))
        {
            attributeControls.push_back(Event("ACBarNoteDuration" + noteDurations[i], 1));
        }
        else
        {
            attributeControls.push_back(Event("ACBarNoteDuration" + noteDurations[i], 0));
        }
    }
    return attributeControls;
}

//------------------------------------------------------------------------
// 								-------------------
// 								| Track level ACs |
// 								-------------------
//------------------------------------------------------------------------

// TrackOnsetPolyphony
//------------------------------------------------------------------------
TrackOnsetPolyphony::TrackOnsetPolyphony(int polyphonyMin_, int polyphonyMax_)
    : polyphonyMin(polyphonyMin_)
    , polyphonyMax(polyphonyMax_)
{
    for (size_t i = polyphonyMin; i < polyphonyMax + 1; ++i)
    {
        tokens.push_back("ACTrackOnsetPolyphonyMin_" + std::to_string(i));
        tokens.push_back("ACTrackOnsetPolyphonyMax_" + std::to_string(i));
    }
}

//------------------------------------------------------------------------
std::vector<Event> TrackOnsetPolyphony::compute(const TrackType &track, int timeDivision,
                                                const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
                                                const std::vector<int> &barsIdx)
{
    // Get note start times
    auto &notes = *track.notes;
    std::vector<int> noteTicks;
    for (NoteType note : notes)
    {
        noteTicks.push_back(static_cast<int>(note.time));
    }

    // Count polyphony at each onset (tick value)
    std::unordered_map<int, int> onsetCounts;
    for (const int tick : noteTicks)
    {
        onsetCounts[tick]++;
    }

    // If there are no notes, set default values
    int onsetPolyMin = 0, onsetPolyMax = 0;
    if (!onsetCounts.empty())
    {
        onsetPolyMin = std::numeric_limits<int>::max();
        onsetPolyMax = std::numeric_limits<int>::min();
        for (const auto &kv : onsetCounts)
        {
            onsetPolyMin = std::min(onsetPolyMin, kv.second);
            onsetPolyMax = std::max(onsetPolyMax, kv.second);
        }
    }

    if (onsetPolyMin > polyphonyMin)
    {
        onsetPolyMin = polyphonyMin;
    }
    // Clamp according to polyphony limits
    int minPoly = std::max(onsetPolyMin, polyphonyMin);
    int maxPoly = std::min(onsetPolyMax, polyphonyMax);
    // Results
    return {Event("ACBarOnsetPolyphonyMin", minPoly, -1), Event("ACBarOnsetPolyphonyMax", maxPoly, -1)};
}

// TrackNoteDensity
//------------------------------------------------------------------------
TrackNoteDensity::TrackNoteDensity(int densityMin_, int densityMax_)
    : densityMin(densityMin_)
    , densityMax(densityMax_)
{
    for (int i = densityMin; i < densityMax; ++i)
    {
        tokens.push_back("ACTrackNoteDensityMin_" + std::to_string(i));
        tokens.push_back("ACTrackNoteDensityMax_" + std::to_string(i));
    }
    tokens.push_back("ACTrackNoteDensityMin_" + std::to_string(densityMax) + "+");
    tokens.push_back("ACTrackNoteDensityMax_" + std::to_string(densityMax) + "+");
}

//------------------------------------------------------------------------
std::vector<Event> TrackNoteDensity::compute(const TrackType &track, int timeDivision,
                                             const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
                                             const std::vector<int> &barsIdx)
{
    // Get note start times
    auto &notes = *track.notes;
    std::vector<int> noteTicks;
    noteTicks.reserve(notes.size());
    for (NoteType note : notes)
    {
        noteTicks.push_back(static_cast<int>(note.time));
    }

    // Make a mutable copy of ticksBars
    std::vector<int> ticksBarsCopy = ticksBars;
    int trackEndTick               = track.end();
    if (trackEndTick > ticksBarsCopy.back())
    {
        ticksBarsCopy.push_back(trackEndTick + 1);
    }

    // Compute histogram: note counts for each bar
    std::vector<size_t> barNoteDensity = libmusictokUtils::histogram(noteTicks, ticksBarsCopy);

    // Find min and max density
    size_t barDensityMin = *std::min_element(barNoteDensity.begin(), barNoteDensity.end());
    size_t barDensityMax = *std::max_element(barNoteDensity.begin(), barNoteDensity.end());

    std::vector<Event> controls;

    if (barDensityMin >= this->densityMax)
    {
        controls.push_back(Event("ACTrackNoteDensityMin", std::to_string(this->densityMax) + "+", -1));
        controls.push_back(Event("ACTrackNoteDensityMax", std::to_string(this->densityMax) + "+", -1));
    }
    else
    {
        controls.push_back(Event("ACTrackNoteDensityMin", std::to_string(barDensityMin), -1));
        if (barDensityMax >= this->densityMax)
        {
            controls.push_back(Event("ACTrackNoteDensityMax", std::to_string(this->densityMax) + "+", -1));
        }
        else
        {
            controls.push_back(Event("ACTrackNoteDensityMax", std::to_string(barDensityMax), -1));
        }
    }

    return controls;
}

// TrackNoteDuration
//------------------------------------------------------------------------
TrackNoteDuration::TrackNoteDuration()
{
    for (std::string duration : noteDurations)
    {
        tokens.push_back("ACBarNoteDuration" + duration + "_0");
        tokens.push_back("ACBarNoteDuration" + duration + "_1");
    }
}

//------------------------------------------------------------------------
std::vector<Event> TrackNoteDuration::compute(const TrackType &track, int timeDivision,
                                              const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
                                              const std::vector<int> &barsIdx)
{
    std::set<double> durations;
    std::vector<Event> attributeControls;
    for (const NoteType &note : *track.notes)
    {
        durations.insert(static_cast<double>(note.duration));
    }

    for (size_t i = 0; i < factors.size(); ++i)
    {
        if (durations.contains(static_cast<double>(timeDivision) * factors[i]))
        {
            attributeControls.push_back(Event("ACBarNoteDuration" + noteDurations[i], 1));
        }
        else
        {
            attributeControls.push_back(Event("ACBarNoteDuration" + noteDurations[i], 0));
        }
    }
    return attributeControls;
}

// TrackRepetition
//------------------------------------------------------------------------
TrackRepetition::TrackRepetition(int numBins_, int numConsecutiveBars_, std::pair<symusic::u8, symusic::u8> pitchRange_)
    : numBins(numBins_)
    , numConsecutiveBars(numConsecutiveBars_)
    , pitchRange(pitchRange_)
{
    bins = libmusictokUtils::linspace(0.0, 1.0, numBins);

    // Create tokens: "ACTrackRepetition_{i:.2f}" for each bin value
    std::vector<std::string> tokens;
    tokens.reserve(bins.size());
    for (const auto &b : bins)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "ACTrackRepetition_%.2f", b);
        tokens.emplace_back(buf);
    }
}

//------------------------------------------------------------------------
std::vector<Event> TrackRepetition::compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                                            const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx)
{
    std::vector<symusic::PianorollMode> modeEnums = {symusic::str_to_pianoroll_mode("onset"),
                                                     symusic::str_to_pianoroll_mode("offset")};

    symusic::TrackPianoroll pianoroll = symusic::TrackPianoroll::from_track(track, modeEnums, pitchRange, false);

    const auto [modeDim, pitchDim, timeDim] = pianoroll.dims();
    auto *data                              = pianoroll.data();

    std::vector<double> similarities;

    for (size_t barIdx = 0; barIdx + 1 < ticksBars.size(); ++barIdx)
    {
        int start1 = ticksBars[barIdx];
        int end1   = ticksBars[barIdx + 1];
        if (static_cast<size_t>(start1) >= timeDim)
        {
            break;
        }
        if (end1 > static_cast<int>(timeDim))
        {
            end1 = static_cast<int>(timeDim);
        }

        int barLen        = end1 - start1;
        int numAssertions = 0;
        // Count nonzero entries in bar1 slice
        for (size_t t = start1; t < static_cast<size_t>(end1); ++t)
        {
            for (size_t p = 0; p < pitchDim; ++p)
            {
                for (size_t m = 0; m < modeDim; ++m)
                {
                    numAssertions += data[m * pitchDim * timeDim + p * timeDim + t] ? 1 : 0;
                }
            }
        }
        if (numAssertions == 0)
        {
            continue;
        }

        // Iterate over next bars up to numConsecutiveBars
        for (size_t bar2Idx = barIdx + 1;
             bar2Idx < ticksBars.size() && bar2Idx < barIdx + static_cast<size_t>(this->numConsecutiveBars); ++bar2Idx)
        {
            int start2 = ticksBars[bar2Idx];
            int end2   = (bar2Idx + 1 < ticksBars.size()) ? ticksBars[bar2Idx + 1] : static_cast<int>(timeDim);
            if (end2 > static_cast<int>(timeDim))
            {
                end2 = static_cast<int>(timeDim);
            }
            int bar2Len = end2 - start2;
            if (barLen != bar2Len)
            {
                continue;
            }

            // Count "AND" (intersection) nonzero
            int intersection = 0;
            for (int i = 0; i < barLen; ++i)
            {
                for (size_t p = 0; p < pitchDim; ++p)
                {
                    for (size_t m = 0; m < modeDim; ++m)
                    {
                        auto val1 = data[m * pitchDim * timeDim + p * timeDim + (start1 + i)];
                        auto val2 = data[m * pitchDim * timeDim + p * timeDim + (start2 + i)];
                        intersection += (val1 && val2) ? 1 : 0;
                    }
                }
            }
            similarities.push_back(static_cast<double>(intersection) / numAssertions);
        }
    }

    if (!similarities.empty())
    {
        double meanSim = std::accumulate(similarities.begin(), similarities.end(), 0.0) / similarities.size();
        // Find closest bin index: assume _bins is a std::vector<double> member
        auto it    = std::min_element(this->bins.begin(), this->bins.end(), [meanSim](double a, double b) {
            return std::abs(a - meanSim) < std::abs(b - meanSim);
        });
        size_t idx = std::distance(this->bins.begin(), it);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", this->bins[idx]);
        return {Event("ACTrackRepetition", buf, -1)};
    }
    return {};
}

} // namespace libmusictok
