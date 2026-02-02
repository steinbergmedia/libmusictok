//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/remi.h
// Created by  : Steinberg, 05/2025
// Description : REMI tokenizer class definition
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
class REMI : public MusicTokenizer
{
public:
    REMI(const std::filesystem::path &tokenizerFile, int maxBarEmbedding = -1, bool verbose = true);

    REMI(TokenizerConfig &tokenizerConfig);

    ~REMI() = default;

private:
    friend class MMM;

    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    int computeTicksPerPos(int ticksPerBeat) const;

    std::tuple<int, int, int> computeTicksPerUnits(symusic::Tick::unit time,
                                                   std::pair<symusic::u8, symusic::u8> currentTimeSig,
                                                   int timeDivision) const;

    std::tuple<int, int> addNewBars(int untilTime, const std::string &eventType, std::vector<Event> &allEvents,
                                    int currentBar, int barAtLastTsChange, int tickAtLastTsChange, int tickAtCurrentBar,
                                    std::pair<symusic::u8, symusic::u8> currentTimeSig, int ticksPerBar) const;

    void addPositionEvent(Event event, std::vector<Event> &allEvents, int tickAtCurrentBar, int ticksPerPos) const;
};

} // namespace libmusictok
