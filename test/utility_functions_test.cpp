//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/utility_functions_test.cpp
// Created by  : Steinberg, 03/2025
// Description : tests the utility functions
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <gtest/gtest.h>

//------------------------------------------------------------------------

#include "libmusictok/utility_functions.h"
#include "testing_utilities.hpp"

//------------------------------------------------------------------------
using namespace libmusictokUtils;
namespace libmusictokTest {

//------------------------------------------------------------------------
class UtilityFunctionsTest : public ::testing::Test
{
};

//------------------------------------------------------------------------
TEST_F(UtilityFunctionsTest, canLoadMidiFile)
{
    std::filesystem::path midiFilePath1 =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType score1 = libmusictokUtils::loadScoreFromMidi(midiFilePath1);

    std::vector<std::shared_ptr<TrackType>> tracksVec = *score1.tracks;
    symusic::pyvec<NoteType> notesVec                 = *tracksVec[1]->notes;
    int firstNotePitch                                = notesVec[0].pitch;

    std::filesystem::path outputSaveFile = std::filesystem::path(resourcesPath) / "outputs" / "midi_dump.mid";
    libmusictokUtils::saveMidiFromScore(score1, outputSaveFile);

    // It is best that the outputs are manually inspected once in a while...
    ASSERT_EQ(1, 1);
}

//------------------------------------------------------------------------
TEST_F(UtilityFunctionsTest, testMergeTracks)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType score1 = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    auto tracksVec     = *score1.tracks;
    auto tracksToMerge = {tracksVec[1], tracksVec[2], tracksVec[3], tracksVec[4]};
    auto mergedTrack   = mergeTracks(tracksToMerge);
    score1.tracks->clear();
    score1.tracks->insert(score1.tracks->begin(), mergedTrack);
    std::filesystem::path outputSaveFile =
        std::filesystem::path(resourcesPath) / "outputs" / "I Gotta Feeling merged.mid";
    libmusictokUtils::saveMidiFromScore(score1, outputSaveFile);

    // manually inspect the MIDI files when possible...
    ASSERT_EQ(1, (*score1.tracks).size());
}

//------------------------------------------------------------------------
TEST_F(UtilityFunctionsTest, testMergeSameProgramTracks)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType score1 = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    (*score1.tracks)[1]->program = 0;
    (*score1.tracks)[2]->program = 0;
    (*score1.tracks)[3]->program = 0;
    mergeSameProgramTracks(score1.tracks);
    std::filesystem::path outputSaveFile =
        std::filesystem::path(resourcesPath) / "outputs" / "I Gotta Feeling merged by program.mid";
    libmusictokUtils::saveMidiFromScore(score1, outputSaveFile);

    // manually inspect the MIDI files when possible...
    ASSERT_EQ(4, (*score1.tracks).size());
}

} // namespace libmusictokTest
