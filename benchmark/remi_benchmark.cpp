//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/remi_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      REMI tokenizer
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <benchmark/benchmark.h>

//------------------------------------------------------------------------

#include "libmusictok/tokenizations/remi.h"

//------------------------------------------------------------------------
namespace libmusictokBenchmark {
using namespace libmusictok;

const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
class REMIBenchmarks : public benchmark::Fixture
{
public:
    REMIBenchmarks()
        : remiTokenizer(tokenizerPath, -1, false)
    {
    }
    void SetUp(::benchmark::State &state) {}
    void TearDown(::benchmark::State &state) {}

protected:
    std::filesystem::path tokenizerPath =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "REMI_tokenizer.json";
    std::filesystem::path tokenizerPathAllStops =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "REMI_tokenizer.json";
    REMI remiTokenizer;
};

//------------------------------------------------------------------------
BENCHMARK_F(REMIBenchmarks, Encode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = remiTokenizer.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(REMIBenchmarks, Decode)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = remiTokenizer.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = remiTokenizer.decode(std::get<TokSequenceVec>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(REMIBenchmarks, ConstructUntrained)(benchmark::State &st)
{
    for (auto _ : st)
    {
        REMI tokenizer = REMI(tokenizerPathAllStops, -1, false);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
