//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/mmm.h
// Created by  : Steinberg, 06/2025
// Description : MMM tokenizer class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/tokenizations/music_tokenizer.h"
#include <memory>

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
class MMM final : public MusicTokenizer
{
public:
    MMM(const std::filesystem::path &tokenizerFile, bool verbose = true);

    MMM(TokenizerConfig &tokenizerConfig);

    ~MMM() = default;

    std::variant<TokSequence, std::vector<TokSequence>> encode(
        const std::filesystem::path &scorePath, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const override;

    std::variant<TokSequence, std::vector<TokSequence>> encode(
        const ScoreType &score, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const override;

    std::variant<TokSequenceVec, TokSequence> encodeWithoutConcatenation(
        const std::filesystem::path &scorePath, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const;

    std::variant<TokSequenceVec, TokSequence> encodeWithoutConcatenation(
        const ScoreType &score, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const;

    void addToVocab(const std::string &token, bool specialToken = false, int vocabIdx = -1,
                    std::u32string byte = std::u32string()) override;

    void loadNewConfigFile(const std::filesystem::path &tokenizerConfigFile) override;

private:
    void loadFromJson(const std::filesystem::path &tokenizerFile) override;

    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    void sortEvents(std::vector<Event> &events) const override;

    std::vector<TokSequence> splitTokSeqPerTrack(const TokSequence &tokSeq, bool keepTrackTokens = false) const;

    // Vars
    std::unique_ptr<MusicTokenizer> baseTokenizer;
};

//------------------------------------------------------------------------
} // namespace libmusictok
