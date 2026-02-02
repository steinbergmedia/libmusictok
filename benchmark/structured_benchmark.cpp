//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/structured_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      Structured tokenizer
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <benchmark/benchmark.h>

//------------------------------------------------------------------------

#include "libmusictok/tokenizations/structured.h"

//------------------------------------------------------------------------
namespace libmusictokBenchmark {
using namespace libmusictok;

const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
class StructuredBenchmarks : public benchmark::Fixture
{
public:
    StructuredBenchmarks()
        : structuredTokenizer(tokenizerPath, false)
    {
    }
    void SetUp(::benchmark::State &state) {}
    void TearDown(::benchmark::State &state) {}

protected:
    std::filesystem::path tokenizerPath =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "structured_tokenizer.json";
    Structured structuredTokenizer;
};

//------------------------------------------------------------------------
BENCHMARK_F(StructuredBenchmarks, Encode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = structuredTokenizer.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(StructuredBenchmarks, Decode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = structuredTokenizer.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = structuredTokenizer.decode(std::get<TokSequence>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(StructuredBenchmarks, ConstructUntrained)(benchmark::State &st)
{
    for (auto _ : st)
    {
        Structured tokenizer = Structured(tokenizerPath, false);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
