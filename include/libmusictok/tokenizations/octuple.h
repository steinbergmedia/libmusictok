//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/octuple.h
// Created by  : Steinberg, 10/2025
// Description : Octuple tokenizer class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/tokenizations/music_tokenizer.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
class Octuple final : public MusicTokenizer
{
public:
    Octuple(const std::filesystem::path &tokenizerFile, bool v);

    Octuple(TokenizerConfig &tokenizerConfig);

    ~Octuple() = default;

private:
    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    std::variant<TokSequence, std::vector<TokSequence>> scoreToTokens(
        const ScoreType &score, const AttributeControlsIndexesType &attributeControlIndexes = {}) const override;

    int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    std::unordered_map<std::string, int> vocabTypesIdx;
};

} // namespace libmusictok
