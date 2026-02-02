//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/test_preprocess.cpp
// Created by  : Steinberg, 09/2025
// Description : Tests the preprocessScore method
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/midilike.h"
#include "libmusictok/tokenizations/music_tokenizer.h"
#include "libmusictok/tokenizations/tsd.h"
#include "testing_utilities.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using namespace libmusictok;
namespace libmusictokTest {

//------------------------------------------------------------------------
class TestPreprocess : public ::testing::TestWithParam<std::tuple<std::string, std::filesystem::path>>
{
};

//------------------------------------------------------------------------
TEST_P(TestPreprocess, PreprocessingIsIdempotent)
{
    const auto &[tokenization, filePath] = GetParam();

    // Create config with config kwargs
    TokenizerConfig config;
    config.useTempos         = true;
    config.useTimeSignatures = true;
    config.useSustainPedals  = true;
    config.usePitchBends     = true;
    config.logTempos         = true;
    config.beatRes           = {
        {  {0, 4}, 8},
        { {4, 12}, 4},
        {{12, 16}, 2}
    };
    config.deleteEqualSuccessiveTimeSigChanges = true;
    config.deleteEqualSuccessiveTempoChanges   = true;

    auto tokenizer = TokenizerFactory::create(tokenization, config);
    ASSERT_NE(tokenizer, nullptr) << "Unknown tokenizer: " << tokenization;

    ScoreType score           = libmusictokUtils::loadScoreFromMidi(filePath);
    ScoreType scoreProcessed1 = tokenizer->preprocessScore(score);
    ScoreType scoreProcessed2 = tokenizer->preprocessScore(scoreProcessed1);

    ASSERT_EQ(scoreProcessed1, scoreProcessed2);
}

//------------------------------------------------------------------------
// Builds the test instantiation tuples: (tokenizer, file_path)
std::vector<std::tuple<std::string, std::filesystem::path>> makeTestCases()
{
    std::vector<std::tuple<std::string, std::filesystem::path>> cases;
    for (const auto &tok : {"MIDILike", "TSD"})
    {
        for (const auto &path : getAllMidiPaths())
        {
            cases.emplace_back(tok, path);
        }
    }
    return cases;
}

//------------------------------------------------------------------------
INSTANTIATE_TEST_SUITE_P(PreprocessTestSuite, TestPreprocess, testing::ValuesIn(makeTestCases()),
                         [](const testing::TestParamInfo<TestPreprocess::ParamType> &info) {
                             const auto &tokenizer = std::get<0>(info.param);
                             const auto &filePath  = std::get<1>(info.param);
                             return "preprocess" + midiFileNameFormatter(filePath) + "with" + tokenizer;
                         });

} // namespace libmusictokTest
