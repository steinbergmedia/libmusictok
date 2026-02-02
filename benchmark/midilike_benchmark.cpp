//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/midilike_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      MIDILike tokenizer
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <benchmark/benchmark.h>

//------------------------------------------------------------------------

#include "libmusictok/tokenizations/midilike.h"

//------------------------------------------------------------------------
namespace libmusictokBenchmark {
using namespace libmusictok;

const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
class MIDILikeBenchmarks : public benchmark::Fixture
{
public:
    MIDILikeBenchmarks()
        : tokenizer(tokenizerPath, false)
    {
    }
    void SetUp(::benchmark::State &state) {}
    void TearDown(::benchmark::State &state) {}

protected:
    std::filesystem::path tokenizerPath =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "MIDILike_tokenizer.json";
    MIDILike tokenizer;
};

//------------------------------------------------------------------------
BENCHMARK_F(MIDILikeBenchmarks, Encode)(benchmark::State &st)
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
BENCHMARK_F(MIDILikeBenchmarks, Decode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = tokenizer.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = tokenizer.decode(std::get<TokSequence>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MIDILikeBenchmarks, ConstructUntrained)(benchmark::State &st)
{
    for (auto _ : st)
    {
        MIDILike tokenizer = MIDILike(tokenizerPath, false);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
