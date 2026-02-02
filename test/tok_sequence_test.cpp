//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/tok_sequence_test.cpp
// Created by  : Steinberg, 03/2025
// Description : TokSequence class unit tests
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tok_sequence.h"
#include "libmusictok/tokenizations/tsd.h"
#include "libmusictok/tokenizer_config.h"
#include "libmusictok/utility_functions.h"
#include "testing_utilities.hpp"

//------------------------------------------------------------------------

#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

//------------------------------------------------------------------------
using namespace libmusictok;
namespace libmusictokTest {
//------------------------------------------------------------------------
class TokSequenceTest : public ::testing::Test
{
protected:
    TokSequenceTest() {}
    virtual void SetUp() {}
};

//------------------------------------------------------------------------
std::vector<int> concatenateVectors(const std::vector<int> &vec1, const std::vector<int> &vec2)
{
    std::vector<int> result;
    result.reserve(vec1.size() + vec2.size()); // Reserve space for efficiency

    result.insert(result.end(), vec1.begin(), vec1.end()); // Insert elements from vec1
    result.insert(result.end(), vec2.begin(), vec2.end()); // Insert elements from vec2

    return result;
}

//------------------------------------------------------------------------
std::vector<std::string> concatenateStringVectors(const std::vector<std::string> &vec1,
                                                  const std::vector<std::string> &vec2)
{
    std::vector<std::string> result;
    result.reserve(vec1.size() + vec2.size()); // Reserve space for efficiency

    result.insert(result.end(), vec1.begin(), vec1.end()); // Insert elements from vec1
    result.insert(result.end(), vec2.begin(), vec2.end()); // Insert elements from vec2

    return result;
}

//------------------------------------------------------------------------
TEST_F(TokSequenceTest, ConcatTest)
{
    std::vector<int> ids1(10);
    std::vector<int> ids2(10);
    std::vector<std::string> str1(10);
    std::vector<std::string> str2(10);

    // Initialize data
    for (int i = 0; i < 10; ++i)
    {
        ids1[i] = i;
        ids2[i] = i + 10;
        str1[i] = std::to_string(i * 2);
        str2[i] = std::to_string((i + 10) * 2);
    }

    std::string _bytes1;
    std::string _bytes2;

    for (const auto &s : str1)
    {
        _bytes1 += s;
    }
    for (const auto &s : str2)
    {
        _bytes2 += s;
    }

    std::u32string bytes1 = libmusictokUtils::utf8_to_utf32(_bytes1);
    std::u32string bytes2 = libmusictokUtils::utf8_to_utf32(_bytes2);

    // Create TokSequence instances
    TokSequence tokseq1;
    tokseq1.ids    = ids1;
    tokseq1.tokens = str1;
    tokseq1.bytes  = bytes1;

    TokSequence tokseq2;
    tokseq2.ids    = ids2;
    tokseq2.tokens = str2;
    tokseq2.bytes  = bytes2;

    // Concatenate sequences
    TokSequence seq_concat = tokseq1 + tokseq2;
    tokseq1 += tokseq2;

    // Assertions
    ASSERT_EQ(concatenateVectors(ids1, ids2), seq_concat.ids.get());
    ASSERT_EQ(concatenateStringVectors(str1, str2), seq_concat.tokens.get());
    ASSERT_EQ(bytes1 + bytes2, seq_concat.bytes.get());
    ASSERT_EQ(tokseq1, seq_concat);
}

//------------------------------------------------------------------------
TEST_F(TokSequenceTest, SliceTest)
{
    TokSequence tokSeq;
    std::vector<int> ids(58);
    std::vector<std::string> tokens(58);
    std::u32string bytes = U"Falsches Üben von Xylophonmusik quält jeden größeren Zwerg";

    for (int i = 0; i < 58; ++i)
    {
        ids[i]    = i;
        tokens[i] = "token_" + std::to_string(i);
    }

    tokSeq.ids    = ids;
    tokSeq.tokens = tokens;
    tokSeq.bytes  = bytes;

    TokSequence grosserenSeq = tokSeq.slice(44, 52);
    TokSequence spaceSeq     = tokSeq.slice(8, 9);
    TokSequence xylophoneSeq = tokSeq.slice(18, 31);

    ASSERT_EQ(U"größeren", grosserenSeq.bytes.get());
    ASSERT_EQ(U" ", spaceSeq.bytes.get());
    ASSERT_EQ(U"Xylophonmusik", xylophoneSeq.bytes.get());

    ASSERT_EQ(8, std::get<int>(spaceSeq.ids[0]));
}

//------------------------------------------------------------------------
TEST_F(TokSequenceTest, SliceConcatTest)
{
    std::vector<int> ids(10);
    std::vector<std::string> str(10);

    // Initialize data
    for (int i = 0; i < 10; ++i)
    {
        ids[i] = i * 2;
        str[i] = std::to_string(i);
    }

    std::string bytes;

    for (const auto &s : str)
    {
        bytes += s;
    }

    // Create TokSequence instance
    TokSequence tokseq;
    tokseq.ids    = ids;
    tokseq.tokens = str;
    tokseq.bytes  = libmusictokUtils::utf8_to_utf32(bytes);

    // Slice TokSequence
    TokSequence subseq1 = tokseq.slice(0, 10);
    TokSequence subseq2 = tokseq.sliceUntilEnd(10);

    // Concatenate sequences
    TokSequence seq_concat = subseq1 + subseq2;

    // Assertions
    ASSERT_EQ(subseq1.ids.get(), std::vector<int>(ids.begin(), ids.begin() + 10));
    ASSERT_EQ(subseq1.tokens.get(), std::vector<std::string>(str.begin(), str.begin() + 10));
    ASSERT_EQ(subseq1.bytes.get(), libmusictokUtils::utf8_to_utf32(std::string(bytes.begin(), bytes.begin() + 10)));

    ASSERT_EQ(subseq2.ids.get(), std::vector<int>(ids.begin() + 10, ids.end()));
    ASSERT_EQ(subseq2.tokens.get(), std::vector<std::string>(str.begin() + 10, str.end()));
    ASSERT_EQ(subseq2.bytes.get(), libmusictokUtils::utf8_to_utf32(std::string(bytes.begin() + 10, bytes.end())));

    ASSERT_EQ(seq_concat, tokseq);
}

//------------------------------------------------------------------------
TEST_F(TokSequenceTest, EqualityTest)
{
    std::vector<int> ids1(10);
    std::vector<int> ids2(10);
    std::vector<std::string> str1(10);
    std::vector<std::string> str2(10);

    // Initialize data
    for (int i = 0; i < 10; ++i)
    {
        ids1[i] = i;
        ids2[i] = i;
        str1[i] = std::to_string(i * 2);
        str2[i] = std::to_string((i + 10) * 2);
    }

    // Create TokSequence instances
    TokSequence tokseq1;
    tokseq1.ids    = ids1;
    tokseq1.tokens = str1;

    TokSequence tokseq2;
    tokseq2.ids    = ids2;
    tokseq2.tokens = str2;

    TokSequence tokseq5;
    tokseq5.ids = ids2;

    // Create empty tokens for comparison
    TokSequence tokseq3;
    TokSequence tokseq4;

    // Assertions
    ASSERT_FALSE(tokseq1 == tokseq2); // Expect false as tokens are different
    ASSERT_FALSE(tokseq2 == tokseq1); // mirror condition

    ASSERT_FALSE(tokseq1 == tokseq3); // Expect false as one empty and other not
    ASSERT_FALSE(tokseq3 == tokseq1); // mirror condition

    ASSERT_FALSE(tokseq3 == tokseq4); // Expect false as both empty
    ASSERT_FALSE(tokseq4 == tokseq3); // mirror condition

    ASSERT_TRUE(tokseq1 == tokseq5); // Expect true as same ids
    ASSERT_TRUE(tokseq5 == tokseq1); // mirror condition
}

//------------------------------------------------------------------------
TEST_F(TokSequenceTest, SizeTest)
{
    std::vector<int> ids1(10);
    std::vector<int> ids2(10);
    std::vector<int> ids3(10);
    std::vector<int> ids4(10);
    std::vector<std::string> str1(20);
    std::vector<std::string> str2(20);
    std::vector<std::string> str3(20);
    std::string bytes1;
    std::string bytes2;
    std::string bytes3;

    // Initialize data
    for (int i = 0; i < 10; ++i)
    {
        ids1[i] = i;
        ids2[i] = i + 10;
        ids3[i] = i + 20;
        ids4[i] = i + 30;
    }
    for (int i = 0; i < 20; ++i)
    {
        str1[i] = std::to_string(i * 2);
        str2[i] = std::to_string((i + 10) * 2);
        str3[i] = std::to_string((i + 20) * 2);
    }
    for (const auto &s : str1)
    {
        bytes1 += s;
    }
    for (const auto &s : str2)
    {
        bytes2 += s;
    }
    for (const auto &s : str3)
    {
        bytes3 += s;
    }

    // Create TokSequence instances:

    // Well behaved tokseq with ids encoded
    TokSequence tokseq1;
    tokseq1.ids           = ids1;
    tokseq1.tokens        = str1;
    tokseq1.bytes         = libmusictokUtils::utf8_to_utf32(bytes1);
    tokseq1.areIdsEncoded = true;

    // non encoded ids - size should be that of tokens
    TokSequence tokseq2;
    tokseq2.ids           = ids2;
    tokseq2.tokens        = str2;
    tokseq2.bytes         = libmusictokUtils::utf8_to_utf32(bytes2);
    tokseq2.areIdsEncoded = false;

    // non-encoded ids and no tokens - falls to size of bytes
    TokSequence tokseq3;
    tokseq3.ids   = ids3;
    tokseq3.bytes = libmusictokUtils::utf8_to_utf32(bytes3);

    // non-encoded ids and nothin else provided - should return 0
    TokSequence tokseq4;
    tokseq4.ids = ids4;

    // Assertions
    ASSERT_EQ(tokseq1.size(), 10);

    ASSERT_EQ(tokseq2.size(), 20);

    ASSERT_EQ(tokseq3.size(), 40);

    ASSERT_EQ(tokseq4.size(), 0);
}

//------------------------------------------------------------------------
class TokSequenceParameterisedTest : public ::testing::TestWithParam<std::filesystem::path>
{
    void SetUp()
    {
        cfg.usePrograms = true;
        tokenizer       = std::make_unique<TSD>(cfg);
    }

public:
    TokenizerConfig cfg;
    std::unique_ptr<TSD> tokenizer;
};

TEST_P(TokSequenceParameterisedTest, splitTokseqPerBarsBeats)
{
    TokSequenceVec tokseqVec = std::get<TokSequenceVec>(tokenizer->encode(GetParam()));

    for (const auto &tokseq : tokseqVec)
    {
        // Split per bars
        std::vector<TokSequence> seqsBar = tokseq.splitPerBars();
        TokSequence concatSeqBar         = seqsBar.front();
        for (size_t i = 1; i < seqsBar.size(); ++i)
        {
            concatSeqBar += seqsBar[i];
        }
        ASSERT_TRUE(concatSeqBar == tokseq);

        // Split per beats
        std::vector<TokSequence> seqsBeat = tokseq.splitPerBeats();
        TokSequence concatSeqBeat         = seqsBeat.front();

        for (size_t i = 1; i < seqsBeat.size(); ++i)
        {
            concatSeqBeat += seqsBeat[i];
        }
        ASSERT_TRUE(concatSeqBeat == tokseq);
    }
}

// Instantiate test suite with list of MIDI files
INSTANTIATE_TEST_SUITE_P(MidiPaths, TokSequenceParameterisedTest, ::testing::ValuesIn(getMidiPathsMultitrack()),
                         [](const ::testing::TestParamInfo<TokSequenceParameterisedTest::ParamType> &info) {
                             return midiFileNameFormatter(info.param.filename());
                         });

//------------------------------------------------------------------------
} // namespace libmusictokTest
