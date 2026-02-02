//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/test_tokenize.cpp
// Created by  : Steinberg, 09/2025
// Description : Tests the various tokenization options
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/midilike.h"
#include "libmusictok/tokenizations/mmm.h"
#include "libmusictok/tokenizations/music_tokenizer.h"
#include "libmusictok/tokenizations/remi.h"
#include "libmusictok/tokenizations/tsd.h"
#include "testing_utilities.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using namespace libmusictok;
namespace libmusictokTest {

//------------------------------------------------------------------------
std::vector<std::pair<std::string, TokenizerConfig>> getTokParamsOneTrack()
{
    TokenizerConfig baseParams = TokenizerConfig(getBaseTokenizerConfig());

    baseParams.useChords                           = false;
    baseParams.logTempos                           = true;
    baseParams.chordUnknown                        = {};
    baseParams.deleteEqualSuccessiveTimeSigChanges = true;
    baseParams.deleteEqualSuccessiveTempoChanges   = true;
    baseParams.additionalParams["max_duration"]    = {20, 0, 4};

    baseParams.useRests              = true;
    baseParams.useTempos             = true;
    baseParams.useTimeSignatures     = true;
    baseParams.useSustainPedals      = true;
    baseParams.usePitchBends         = true;
    baseParams.usePitchIntervals     = true;
    baseParams.removeDuplicatedNotes = true;

    std::vector<std::pair<std::string, TokenizerConfig>> tokParamsOneTrack;

    for (std::string tokenization : getAllTokenizations())
    {
        if (tokenization == "MMM")
        {
            for (std::string subTokenization : getMMMBaseTokenizations())
            {
                auto params                               = TokenizerConfig(baseParams);
                params.additionalParams["base_tokenizer"] = subTokenization;
                adjustTokParamsForTests(tokenization, params);
                tokParamsOneTrack.push_back({tokenization, params});
            }
        }
        else
        {
            auto params = TokenizerConfig(baseParams);
            adjustTokParamsForTests(tokenization, params);
            tokParamsOneTrack.push_back({tokenization, params});
        }
    }

    return tokParamsOneTrack;
}

//------------------------------------------------------------------------
std::vector<std::pair<std::string, TokenizerConfig>> getTokParamsMultitrack()
{
    TokenizerConfig baseParams = TokenizerConfig(getBaseTokenizerConfig());

    baseParams.useChords                 = true;
    baseParams.useRests                  = true;
    baseParams.useTempos                 = true;
    baseParams.useTimeSignatures         = true;
    baseParams.useSustainPedals          = true;
    baseParams.usePitchBends             = true;
    baseParams.usePrograms               = true;
    baseParams.sustainPedalDuration      = false;
    baseParams.oneTokenStreamForPrograms = true;
    baseParams.programChanges            = false;

    std::vector<std::string> tokenizationsNonOneStream = {"TSD", "REMI", "MIDILike", "Structured", "CPWord", "Octuple"};

    std::vector<std::string> tokenizationsProgramChange = {"TSD", "REMI", "MIDILike"};

    std::vector<std::pair<std::string, TokenizerConfig>> tokParamsMultitrack;

    for (std::string tokenization : getAllTokenizations())
    {
        if (tokenization == "MMM")
        {
            for (std::string subTokenization : getMMMBaseTokenizations())
            {
                auto params                               = TokenizerConfig(baseParams);
                params.additionalParams["base_tokenizer"] = subTokenization;
                adjustTokParamsForTests(tokenization, params);
                tokParamsMultitrack.push_back({tokenization, params});
            }
        }
        else
        {
            auto params = TokenizerConfig(baseParams);
            adjustTokParamsForTests(tokenization, params);
            tokParamsMultitrack.push_back({tokenization, params});
        }

        // if tokenization in tokenizationNonOneStream
        if (std::find(tokenizationsNonOneStream.begin(), tokenizationsNonOneStream.end(), tokenization) !=
            tokenizationsNonOneStream.end())
        {
            TokenizerConfig paramsTmp           = TokenizerConfig(baseParams);
            paramsTmp.oneTokenStreamForPrograms = false;
            if (tokenization == "Octuple")
            {
                paramsTmp.useTempos = false;
            }
            adjustTokParamsForTests(tokenization, paramsTmp);
            tokParamsMultitrack.push_back({tokenization, paramsTmp});
        }

        // if tokenization in tokenizationsProgramChange
        if (std::find(tokenizationsProgramChange.begin(), tokenizationsProgramChange.end(), tokenization) !=
            tokenizationsProgramChange.end())
        {
            TokenizerConfig paramsTmp = TokenizerConfig(baseParams);
            paramsTmp.programChanges  = true;
            adjustTokParamsForTests(tokenization, paramsTmp);
            tokParamsMultitrack.push_back({tokenization, paramsTmp});
        }
    }
    return tokParamsMultitrack;
}

//------------------------------------------------------------------------
void testTokenize(std::filesystem::path filePath, std::pair<std::string, TokenizerConfig> tokParams,
                  bool savingErroneousFiles = false)
{
    ScoreType score;
    try
    {
        score = libmusictokUtils::loadScoreFromMidi(filePath);
    }
    catch (const std::runtime_error err)
    {
        // TODO: Find a way to skip this run from the test suite
        std::clog << "Error when loading " << filePath.string() << ": " << err.what() << std::endl;
        GTEST_SKIP();
    }

    std::shared_ptr<MusicTokenizer> tokenizer = TokenizerFactory::create(tokParams.first, tokParams.second);

    auto [scoreDecoded, scoreRef, hasErrors] = tokenizeAndCheckEquals(score, tokenizer, filePath.stem());

    if (hasErrors && savingErroneousFiles)
    {
        std::filesystem::path outputSaveFileRef =
            "resources/outputs/" + filePath.stem().string() + "_" + tokParams.first + ".mid";
        std::filesystem::path outputSaveFileDecoded =
            "resources/outputs/" + filePath.stem().string() + "_" + tokParams.first + "_decoded.mid";
        libmusictokUtils::saveMidiFromScore(scoreRef, outputSaveFileRef);
        libmusictokUtils::saveMidiFromScore(scoreDecoded, outputSaveFileDecoded);
    }
    ASSERT_FALSE(hasErrors);
}

//------------------------------------------------------------------------
class OneTrackTestTokenize
    : public ::testing::TestWithParam<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>>
{
};
class MultiTrackTestTokenize
    : public ::testing::TestWithParam<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>>
{
};

//------------------------------------------------------------------------
TEST_P(OneTrackTestTokenize, OneTrackMidiToTokensToMidi)
{
    const auto &[filePath, tokParams] = GetParam();
    testTokenize(filePath, tokParams);
}

//------------------------------------------------------------------------
TEST_P(MultiTrackTestTokenize, MultiTrackMidiToTokensToMidi)
{
    const auto &[filePath, tokParams] = GetParam();
    testTokenize(filePath, tokParams);
}

//------------------------------------------------------------------------
// Builds the test instantiation tuples for oneTrack: (filePath, tokParams)
std::vector<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>> makeTestCasesOneTrack()
{
    std::vector<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>> cases;
    for (const auto &path : getMidiPathsOneTrack())
    {
        for (const auto &params : getTokParamsOneTrack())
        {
            cases.emplace_back(path, params);
        }
    }

    return cases;
}

//------------------------------------------------------------------------
// Builds the test instantiation tuples for multitrack: (filePath, tokParams)
std::vector<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>> makeTestCasesMultitrack()
{
    std::vector<std::tuple<std::filesystem::path, std::pair<std::string, TokenizerConfig>>> cases;
    for (const auto &path : getMidiPathsMultitrack())
    {
        for (const auto &params : getTokParamsMultitrack())
        {
            cases.emplace_back(path, params);
        }
    }

    return cases;
}

//------------------------------------------------------------------------
INSTANTIATE_TEST_SUITE_P(
    OneTrackTestSuite, OneTrackTestTokenize, testing::ValuesIn(makeTestCasesOneTrack()),
    [](const testing::TestParamInfo<OneTrackTestTokenize::ParamType> &info) {
        std::string tokenizer = (std::get<1>(info.param)).first;
        const auto &filePath  = (std::get<0>(info.param)).stem();
        if (tokenizer == "MMM")
        {
            tokenizer = "MMM" + std::get<1>(info.param).second.additionalParams.at("base_tokenizer").get<std::string>();
        }

        return tokenizer + midiFileNameFormatter(filePath);
    });

//------------------------------------------------------------------------
INSTANTIATE_TEST_SUITE_P(
    MultiTrackTestSuite, MultiTrackTestTokenize, testing::ValuesIn(makeTestCasesMultitrack()),
    [](const testing::TestParamInfo<MultiTrackTestTokenize::ParamType> &info) {
        std::string tokenizer = (std::get<1>(info.param)).first;
        const auto &filePath  = (std::get<0>(info.param)).stem();
        if (tokenizer == "MMM")
        {
            tokenizer = "MMM" + std::get<1>(info.param).second.additionalParams.at("base_tokenizer").get<std::string>();
        }

        if (std::get<1>(info.param).second.oneTokenStreamForPrograms)
        {
            tokenizer = tokenizer + "OneTokenStream";
        }

        if (std::get<1>(info.param).second.programChanges)
        {
            tokenizer = tokenizer + "WithProgramChanges";
        }

        return tokenizer + midiFileNameFormatter(filePath);
    });

//------------------------------------------------------------------------
} // namespace libmusictokTest
