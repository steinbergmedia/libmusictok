//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/common.h
// Created by  : Steinberg, 11/2025
// Description : common header for libmusictok
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "symusic.h"

namespace libmusictok {

using ScoreType         = symusic::Score<symusic::Tick>;
using TrackType         = symusic::Track<symusic::Tick>;
using NoteType          = symusic::Note<symusic::Tick>;
using TempoType         = symusic::Tempo<symusic::Tick>;
using TimeSigType       = symusic::TimeSignature<symusic::Tick>;
using PedalType         = symusic::Pedal<symusic::Tick>;
using PitchBendType     = symusic::PitchBend<symusic::Tick>;
using ControlChangeType = symusic::ControlChange<symusic::Tick>;

} // namespace libmusictok
