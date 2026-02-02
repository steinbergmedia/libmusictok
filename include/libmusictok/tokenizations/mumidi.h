//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/mumidi.h
// Created by  : Steinberg, 10/2025
// Description : MuMIDI tokenizer class definition
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
class MuMIDI final : public MusicTokenizer
{
public:
    MuMIDI(const std::filesystem::path &tokenizerFile, bool v);

    MuMIDI(TokenizerConfig &tokenizerConfig);

    ~MuMIDI() = default;

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

    std::vector<std::vector<Event>> trackToTokens(std::shared_ptr<TrackType> track,
                                                  std::vector<std::pair<int, int>> ticksPerBeat = {}) const;

    // Vars
    std::unordered_map<std::string, int> vocabTypesIdx = {
        {         "Pitch", 0},
        {     "PitchDrum", 0},
        {      "Position", 0},
        {           "Bar", 0},
        {       "Program", 0},
        {     "BarPosEnc", 1},
        {"PositionPosEnc", 2}
    };
};

} // namespace libmusictok
