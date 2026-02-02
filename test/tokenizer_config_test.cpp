//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/tokenizer_config_test.cpp
// Created by  : Steinberg, 03/2025
// Description : TokenizerConfig class unit tests
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/utility_functions.h"
#include "testing_utilities.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <utility>

//------------------------------------------------------------------------

#include "libmusictok/tokenizer_config.h"

//------------------------------------------------------------------------
using namespace libmusictok;
namespace libmusictokTest {
//------------------------------------------------------------------------

class TokenizerConfigTest : public ::testing::Test
{
protected:
    TokenizerConfigTest() {}
    virtual void SetUp() {}
    TokenizerConfig tokenizerConfig;
    std::filesystem::path createdConfigJsonFilePath =
        std::filesystem::path(resourcesPath) / "tokenizer_configs" / "test_configs.json";
    std::filesystem::path outTestJsonFilePath = std::filesystem::path(resourcesPath) / "outputs" / "output_test.json";
};

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, canInit)
{
    // Check initialised parameters
    std::pair<int, int> pitchRange = {21, 109};
    ASSERT_EQ(pitchRange, tokenizerConfig.pitchRange);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, canInitFromJson)
{
    auto config = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);

    // Check initialised parameters
    std::pair<int, int> pitchRange             = {60, 72};
    std::map<std::pair<int, int>, int> beatRes = {
        { {0, 4}, 8},
        {{4, 12}, 4}
    };
    int numVelocities                                                = 32;
    bool removeDuplicatedNotes                                       = false;
    std::string encodeIdsSplit                                       = "bar";
    std::vector<std::string> specialTokens                           = {"PAD_None",  "BOS_None",   "EOS_None",
                                                                        "MASK_None", "Genre_Rock", "Genre_Jazz"};
    bool useChords                                                   = false;
    bool useRests                                                    = true;
    bool useTimeSignatures                                           = true;
    bool usePrograms                                                 = true;
    int numTempos                                                    = 64;
    std::pair<int, int> tempoRange                                   = {70, 160};
    bool usePitchIntervals                                           = true;
    bool deleteEqualSuccessiveTempoChanges                           = true;
    std::vector<std::pair<int, std::vector<int>>> timeSignatureRange = {
        {8,         {3, 12, 6, 9}},
        {4, {5, 6, 3, 2, 1, 4, 7}}
    };

    ASSERT_EQ(pitchRange, config.pitchRange);
    ASSERT_EQ(beatRes, config.beatRes);
    ASSERT_EQ(numVelocities, config.numVelocities);
    ASSERT_EQ(removeDuplicatedNotes, config.removeDuplicatedNotes);
    ASSERT_EQ(encodeIdsSplit, config.encodeIdsSplit);
    ASSERT_EQ(specialTokens, config.specialTokens);
    ASSERT_EQ(useChords, config.useChords);
    ASSERT_EQ(useRests, config.useRests);
    ASSERT_EQ(useTimeSignatures, config.useTimeSignatures);
    ASSERT_EQ(usePrograms, config.usePrograms);
    ASSERT_EQ(numTempos, config.numTempos);
    ASSERT_EQ(tempoRange, config.tempoRange);
    ASSERT_EQ(usePitchIntervals, config.usePitchIntervals);
    ASSERT_EQ(deleteEqualSuccessiveTempoChanges, config.deleteEqualSuccessiveTempoChanges);
    ASSERT_EQ(timeSignatureRange, config.timeSignatureRange);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, equalityTest)
{
    nlohmann::ordered_json configJson1 = libmusictokUtils::loadOrderedJson(createdConfigJsonFilePath);
    auto config1                       = TokenizerConfig::fromDict(configJson1);
    auto config2                       = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);

    ASSERT_TRUE(config1 == config2);
    ASSERT_FALSE(config1 == tokenizerConfig);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, canConvertToJson)
{
    auto config                            = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);
    nlohmann::ordered_json configJsonTest  = config.toDict();
    std::pair<int, int> pitchRangeExpected = {60, 72};
    std::pair<int, int> pitchRangeTest     = configJsonTest["pitch_range"];
    ASSERT_EQ(pitchRangeExpected, pitchRangeTest);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, inputJsonMatchesOutoutJson)
{
    nlohmann::ordered_json configJsonReference = libmusictokUtils::loadOrderedJson(createdConfigJsonFilePath);

    auto config = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);

    nlohmann::ordered_json configJsonTest = config.toDict();

    ASSERT_EQ(configJsonReference, configJsonTest);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, canSaveToJson)
{
    auto config = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);
    config.saveToJson(outTestJsonFilePath);

    auto savedConfig = TokenizerConfig::loadFromJson(outTestJsonFilePath);

    nlohmann::ordered_json configJsonReference = config.toDict();
    nlohmann::ordered_json configJsonSaved     = savedConfig.toDict();
    ASSERT_EQ(configJsonReference, configJsonSaved);
}

//------------------------------------------------------------------------
TEST_F(TokenizerConfigTest, canBeCopied)
{
    auto config           = TokenizerConfig::loadFromJson(createdConfigJsonFilePath);
    auto configCopy       = config.copy();
    configCopy.pitchRange = {21, 109};

    ASSERT_NE(config.pitchRange, configCopy.pitchRange);
}

} // namespace libmusictokTest
