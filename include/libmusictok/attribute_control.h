//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/attribute_control.h
// Created by  : Steinberg, 07/2025
// Description : AttributeControl class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/common.h"
#include "libmusictok/event.h"
#include "libmusictok/utility_functions.h"
#include "pyvec.hpp"
#include "symusic.h"

#include <iostream>
#include <string>
#include <tuple>

namespace libmusictok {

//------------------------------------------------------------------------
class AttributeControl
{
public:
    AttributeControl(std::vector<std::string> tokens_);

    AttributeControl() = default;

    virtual ~AttributeControl();

    virtual std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                                       const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) = 0;

    // Vars
    std::vector<std::string> tokens;
};

//------------------------------------------------------------------------
// 								-----------------
// 								| Bar level ACs |
// 								-----------------
//------------------------------------------------------------------------

//------------------------------------------------------------------------
class BarAttributeControl : public AttributeControl
{
public:
    // Inherit constructors
    using AttributeControl::AttributeControl;

    ///< compute method
    std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                               const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) override;

    ///< computeOnBar method
    virtual std::vector<Event> computeOnBar(const std::vector<NoteType> &notesBar,
                                            const std::vector<ControlChangeType> &controlsBar,
                                            const std::vector<PitchBendType> &pitchBendsBar,
                                            const int &timeDivision) = 0;
};

//------------------------------------------------------------------------
class BarNoteDensity : public BarAttributeControl
{
public:
    BarNoteDensity(int densityMax_);

    std::vector<Event> computeOnBar(const std::vector<NoteType> &notesBar,
                                    const std::vector<ControlChangeType> &controlsBar,
                                    const std::vector<PitchBendType> &pitchBendsBar, const int &timeDivision) override;

private:
    int densityMax;
};

//------------------------------------------------------------------------
class BarOnsetPolyphony : public BarAttributeControl
{
public:
    BarOnsetPolyphony(int polyphonyMin_, int polyphonyMax_);

    std::vector<Event> computeOnBar(const std::vector<NoteType> &notesBar,
                                    const std::vector<ControlChangeType> &controlsBar,
                                    const std::vector<PitchBendType> &pitchBendsBar, const int &timeDivision) override;

private:
    int polyphonyMin;
    int polyphonyMax;
};

//------------------------------------------------------------------------
class BarPitchClass : public BarAttributeControl
{
public:
    BarPitchClass();

    std::vector<Event> computeOnBar(const std::vector<NoteType> &notesBar,
                                    const std::vector<ControlChangeType> &controlsBar,
                                    const std::vector<PitchBendType> &pitchBendsBar, const int &timeDivision) override;
};

//------------------------------------------------------------------------
class BarNoteDuration : public BarAttributeControl
{
public:
    BarNoteDuration();

    std::vector<Event> computeOnBar(const std::vector<NoteType> &notesBar,
                                    const std::vector<ControlChangeType> &controlsBar,
                                    const std::vector<PitchBendType> &pitchBendsBar, const int &timeDivision) override;

private:
    std::vector<std::string> noteDurations = {"Whole", "Half", "Quarter", "Eight", "Sixteenth", "ThirtySecond"};
    std::vector<double> factors            = {4, 2, 1, 0.5, 0.25};
};

//------------------------------------------------------------------------
// 								-------------------
// 								| Track level ACs |
// 								-------------------
//------------------------------------------------------------------------

class TrackNoteDensity : public AttributeControl
{
public:
    TrackNoteDensity(int densityMin_, int densityMax_);

    std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                               const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) override;

private:
    int densityMin;
    int densityMax;
};

//------------------------------------------------------------------------
class TrackOnsetPolyphony : public AttributeControl
{
public:
    TrackOnsetPolyphony(int polyphonyMin_, int polyphonyMax_);

    std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                               const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) override;

private:
    int polyphonyMin;
    int polyphonyMax;
};

//------------------------------------------------------------------------
class TrackNoteDuration : public AttributeControl
{
public:
    TrackNoteDuration();

    std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                               const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) override;

private:
    std::vector<std::string> noteDurations = {"Whole", "Half", "Quarter", "Eight", "Sixteenth", "ThirtySecond"};
    std::vector<double> factors            = {4, 2, 1, 0.5, 0.25};
};

//------------------------------------------------------------------------
class TrackRepetition : public AttributeControl
{
public:
    TrackRepetition(int numBins_, int numConsecutiveBars_, std::pair<symusic::u8, symusic::u8> pitchRange_);

    std::vector<Event> compute(const TrackType &track, int timeDivision, const std::vector<int> &ticksBars,
                               const std::vector<int> &ticksBeats, const std::vector<int> &barsIdx) override;

private:
    int numBins;
    int numConsecutiveBars;
    std::pair<symusic::u8, symusic::u8> pitchRange;
    std::vector<double> bins;
};

} // namespace libmusictok
