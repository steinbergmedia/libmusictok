//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/cpword.h
// Created by  : Steinberg, 10/2025
// Description : CP-Word tokenizer class definition
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
class CPWord final : public MusicTokenizer
{
public:
    CPWord(const std::filesystem::path &tokenizerFile, bool v);

    CPWord(TokenizerConfig &tokenizerConfig);

    ~CPWord() = default;

private:
    void tweakConfigBeforeCreatingVoc() override;

    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() override;

    std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() override;

    ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                            const std::vector<std::tuple<int, bool>> &programs = {}) const override;

    int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const override;

    std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const override;

    std::unordered_map<std::string, size_t> vocabTypesIdx = {};
};

//------------------------------------------------------------------------
// Helper class for CP-Word
//------------------------------------------------------------------------

//------------------------------------------------------------------------
class CpTokenBuilder
{
private:
    int mTime;
    std::string mDesc = "";
    bool mBar         = false;
    std::optional<int> mPos;
    std::optional<int> mPitch;
    std::optional<int> mVel;
    std::optional<std::string> mDur;
    std::optional<std::string> mChord;
    std::optional<std::string> mRest;
    std::optional<float> mTempo;
    std::optional<std::string> mTimeSig;
    std::optional<int> mProgram;
    bool mPitchDrum = false;

public:
    explicit CpTokenBuilder(int time, const std::string &desc = "")
        : mTime(time)
        , mDesc(desc)
    {
    }

    // Method chaining setters
    CpTokenBuilder &asBar()
    {
        mBar = true;
        return *this;
    }
    CpTokenBuilder &withPosition(int pos)
    {
        mPos = pos;
        return *this;
    }
    CpTokenBuilder &withPitch(int pitch)
    {
        mPitch = pitch;
        return *this;
    }
    CpTokenBuilder &withVelocity(int vel)
    {
        if (vel >= 0)
            mVel = vel;

        return *this;
    }
    CpTokenBuilder &withDuration(const std::string &dur)
    {
        if (dur != "")
            mDur = dur;

        return *this;
    }
    CpTokenBuilder &withChord(const std::string &chord)
    {
        if (chord != "")
            mChord = chord;

        return *this;
    }
    CpTokenBuilder &withRest(const std::string &rest)
    {
        mRest = rest;
        return *this;
    }
    CpTokenBuilder &withTempo(float tempo)
    {
        if (tempo >= 0)
            mTempo = tempo;

        return *this;
    }
    CpTokenBuilder &withTimeSignature(const std::string &timeSig)
    {
        mTimeSig = timeSig;
        return *this;
    }
    CpTokenBuilder &withProgram(int program)
    {
        if (program != -2)
            mProgram = program;

        return *this;
    }
    CpTokenBuilder &asDrumPitch(bool isDrumPitch)
    {
        mPitchDrum = isDrumPitch;
        return *this;
    }

    // Build the final token
    std::vector<Event> build(const TokenizerConfig &config,
                             const std::unordered_map<std::string, size_t> &vocabTypesIdx);
};

} // namespace libmusictok
