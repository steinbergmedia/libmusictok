//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/music_tokenizer_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      Base tokenizer (preprocessScore method)
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
class MusicTokenizerBenchmarks : public benchmark::Fixture
{
public:
    MusicTokenizerBenchmarks()
        : tokenizer(configFile, -1, false)
    {
    }
    void SetUp(::benchmark::State &state) {}
    void TearDown(::benchmark::State &state) {}

protected:
    std::filesystem::path configFile =
        std::filesystem::path(resourcesPath) / "tokenizer_configs" / "tokenizer_config_for_preprocess_score_test.json";
    REMI tokenizer;
};

//------------------------------------------------------------------------
BENCHMARK_F(MusicTokenizerBenchmarks, preprocessScore)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToPreprocess = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        ScoreType preprocessedScore = tokenizer.preprocessScore(scoreToPreprocess);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
