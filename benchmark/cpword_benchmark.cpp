//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/cpword_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      CP-Word tokenizer
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <benchmark/benchmark.h>

//------------------------------------------------------------------------

#include "libmusictok/tokenizations/cpword.h"

//------------------------------------------------------------------------
namespace libmusictokBenchmark {
using namespace libmusictok;

const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
class CPWordBenchmarks : public benchmark::Fixture
{
public:
    CPWordBenchmarks()
        : tokenizer(tokenizerPath, false)
    {
    }
    void SetUp(::benchmark::State &state) {}
    void TearDown(::benchmark::State &state) {}

protected:
    std::filesystem::path tokenizerPath =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "cpword_tokenizer.json";
    CPWord tokenizer;
};

//------------------------------------------------------------------------
BENCHMARK_F(CPWordBenchmarks, Encode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = tokenizer.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(CPWordBenchmarks, Decode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = tokenizer.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = tokenizer.decode(std::get<TokSequenceVec>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(CPWordBenchmarks, ConstructUntrained)(benchmark::State &st)
{
    for (auto _ : st)
    {
        CPWord tokenizer = CPWord(tokenizerPath, false);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
