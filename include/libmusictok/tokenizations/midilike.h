//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/midilike.h
// Created by  : Steinberg, 07/2025
// Description : MIDILike tokenizer class definition
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
class MIDILike final : public MusicTokenizer
{
public:
    MIDILike(const std::filesystem::path &tokenizerFile, bool v);

    MIDILike(TokenizerConfig &tokenizerConfig);

    ~MIDILike() = default;

private:
    friend class MMM;

    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    int eventOrder(const Event &event) const;

    void sortEvents(std::vector<Event> &events) const override;

    void clearActiveNotes(
        bool oneTokenStreamForPrograms, int maxDuration,
        std::unordered_map<int, std::unordered_map<int, std::vector<std::pair<int, int>>>> &activeNotes,
        std::map<int, TrackType> &tracks, TrackType *currentTrack) const;
};

} // namespace libmusictok
