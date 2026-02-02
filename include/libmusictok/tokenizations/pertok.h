//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/pertok.h
// Created by  : Steinberg, 10/2025
// Description : PerTok tokenizer class definition
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
class PerTok final : public MusicTokenizer
{
public:
    PerTok(const std::filesystem::path &tokenizerFile, bool v);

    PerTok(TokenizerConfig &tokenizerConfig);

    ~PerTok() = default;

private:
    void postConstructorChecks();

    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    ScoreType resampleScore(const ScoreType &score, int newTpq,
                            symusic::pyvec<TimeSigType> &timeSignaturesCopy) const override;

    void adjustDurations(symusic::pyvec<NoteType> &notes,
                         const std::vector<std::pair<int, int>> &ticksPerBeat) const override;

    void adjustDurations(symusic::pyvec<PedalType> &pedals,
                         const std::vector<std::pair<int, int>> &ticksPerBeat) const override;

    Event createDurationEvent(const NoteType &note, int program, const std::vector<std::pair<int, int>> &ticksPerBeat,
                              int tpbIdx) const override;

    std::tuple<int, int, int> getClosestDurationTuple(int target) const;

    template <typename T> T getClosestArrayValue(T value, const std::vector<T> &array) const;

    int findClosestPositionTok(int target) const;

    std::vector<std::tuple<int, int, int>> createDurationTuples() override;

    std::vector<int> createMicrotimingTickValues() const;

    std::vector<int> createTimeshiftTickValues() const;

    std::vector<int> createPositionTokLocations() const;

    static int convertDurationsToTicks(const std::string &durationStr);

    // Vars
    std::vector<std::string> microTimeEvents = {"Pitch",     "Pedal", "PedalOff",  "PitchIntervalChord",
                                                "PitchBend", "Chord", "PitchDrum", "Program"};

    bool useMicrotiming  = false;
    bool usePositionToks = false;
    int tpq              = 0;
    int maxMtShift       = 0;
    int minTimeShift     = 0;
    std::vector<int> microtimingTickValues;
    std::vector<int> timeshiftTickValues;
    std::vector<int> positionLocations;
};

} // namespace libmusictok
