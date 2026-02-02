//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Benchmarks
// Filename    : benchmark/mmm_benchmark.cpp
// Created by  : Steinberg, 10/2025
// Description : Benchmarks the initialization and tokenization of the
//      MMM tokenizer
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <benchmark/benchmark.h>

//------------------------------------------------------------------------

#include "libmusictok/tokenizations/mmm.h"

//------------------------------------------------------------------------
namespace libmusictokBenchmark {
using namespace libmusictok;

const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
class MMMBenchmarks : public benchmark::Fixture
{
public:
    MMMBenchmarks()
        : mmmTokenizerRemi(tokenizerPathRemi, false)
        , mmmTokenizerTsd(tokenizerPathTsd, false)
        , mmmTokenizerMidilike(tokenizerPathMidilike, false)
    {
    }

protected:
    std::filesystem::path tokenizerPathRemi =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "MMM_tokenizer.json";
    std::filesystem::path tokenizerPathTsd =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "MMM_tokenizer_TSD_base.json";
    std::filesystem::path tokenizerPathMidilike =
        std::filesystem::path(resourcesPath) / "music_tokenizers" / "MMM_tokenizer_MIDILike_base.json";
    MMM mmmTokenizerRemi;
    MMM mmmTokenizerTsd;
    MMM mmmTokenizerMidilike;
};

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, EncodeUntrainedRemi)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = mmmTokenizerRemi.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, DecodeUntrainedRemi)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = mmmTokenizerRemi.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = mmmTokenizerRemi.decode(std::get<TokSequence>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, ConstructUntrainedRemi)(benchmark::State &st)
{
    for (auto _ : st)
    {
        MMM tokenizer = MMM(tokenizerPathRemi, false);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, EncodeUntrainedTsd)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = mmmTokenizerTsd.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, DecodeUntrainedTsd)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = mmmTokenizerTsd.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = mmmTokenizerTsd.decode(std::get<TokSequence>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, ConstructUntrainedTsd)(benchmark::State &st)
{
    for (auto _ : st)
    {
        MMM tokenizer = MMM(tokenizerPathTsd, false);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, EncodeUntrainedMidiLike)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);

    for (auto _ : st)
    {
        auto tokens = mmmTokenizerMidilike.encode(scoreToTokenize);
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, DecodeUntrainedMIDILike)(benchmark::State &st)
{
    std::filesystem::path midiFilePath =
        std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs" / "I Gotta Feeling.mid";
    ScoreType scoreToTokenize = libmusictokUtils::loadScoreFromMidi(midiFilePath);
    auto tokens               = mmmTokenizerMidilike.encode(scoreToTokenize);

    for (auto _ : st)
    {
        ScoreType decodedScore = mmmTokenizerMidilike.decode(std::get<TokSequence>(tokens));
    }
}

//------------------------------------------------------------------------
BENCHMARK_F(MMMBenchmarks, ConstructUntrainedMIDILike)(benchmark::State &st)
{
    for (auto _ : st)
    {
        MMM tokenizer = MMM(tokenizerPathMidilike, false);
    }
}

//------------------------------------------------------------------------
} // namespace libmusictokBenchmark
