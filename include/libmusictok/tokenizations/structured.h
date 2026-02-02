//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/structured.h
// Created by  : Steinberg, 10/2025
// Description : Structured tokenizer class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/tokenizations/music_tokenizer.h"
#include <tuple>

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
class Structured final : public MusicTokenizer
{
public:
    Structured(const std::filesystem::path &tokenizerFile, bool v);

    Structured(TokenizerConfig &tokenizerConfig);

    ~Structured() = default;

private:
    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    std::variant<TokSequence, std::vector<TokSequence>> scoreToTokens(
        const ScoreType &score, const AttributeControlsIndexesType &attributeControlIndexes = {}) const override;

    std::vector<Event> createTrackEvents(const TrackType &track,
                                         const std::vector<std::pair<int, int>> &ticksPerBeat = {},
                                         symusic::i32 timeDivision = 0, const std::vector<int> &ticksBars = {},
                                         const std::vector<int> &ticksBeats = {},
                                         const std::unordered_map<int, std::variant<std::vector<int>, bool>>
                                             &attributeControlIndexes = {}) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;
};

} // namespace libmusictok
