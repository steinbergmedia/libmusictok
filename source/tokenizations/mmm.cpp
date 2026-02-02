//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/mmm.cpp
// Created by  : Steinberg, 06/2025
// Description : MMM tokenizer class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/mmm.h"
#include "libmusictok/tokenizations/midilike.h"
#include "libmusictok/tokenizations/remi.h"
#include "libmusictok/tokenizations/tsd.h"
#include "libmusictok/utility_functions.h"

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
MMM::MMM(const std::filesystem::path &tokenizerFile, bool v)
    : MusicTokenizer(TokenizerType::kMMM, v)
{
    loadFromJson(tokenizerFile);
    updateTokenizer();
    oneTokenStream = true;
}

//------------------------------------------------------------------------
MMM::MMM(TokenizerConfig &tokConfig)
    : MusicTokenizer(TokenizerType::kMMM, false)
{
    config = tokConfig;
    tweakConfigBeforeCreatingVoc();

    nlohmann::json emptyJson;
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(emptyJson, &config, verbose);
    updateTokenizer();
    oneTokenStream = true;
}

//------------------------------------------------------------------------
void MMM::addToVocab(const std::string &token, bool specialToken, int vocabIdx, std::u32string byte)
{
    if (baseTokenizer->type == TokenizerType::kREMI)
    {
        auto remi = static_cast<REMI *>(baseTokenizer.get());
        remi->addToVocab(token, specialToken, vocabIdx, byte);
    }
    else if (baseTokenizer->type == TokenizerType::kTSD)
    {
        auto tsd = static_cast<TSD *>(baseTokenizer.get());
        tsd->addToVocab(token, specialToken, vocabIdx, byte);
    }
    else if (baseTokenizer->type == TokenizerType::kMIDILike)
    {
        auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
        midiLike->addToVocab(token, specialToken, vocabIdx, byte);
    }
    MusicTokenizer::addToVocab(token, specialToken, vocabIdx, byte);
}

//------------------------------------------------------------------------
void MMM::loadFromJson(const std::filesystem::path &tokenizerFile)
{
    nlohmann::ordered_json jsonTokenizer = libmusictokUtils::loadJson(tokenizerFile);
    config                               = TokenizerConfig::fromDict(jsonTokenizer["config"]);

    // Tweak the tokenizer's configuration and / or attributes before creating the
    // vocabulary. This method is intended to be overridden by inheriting tokenizer
    // classes.
    tweakConfigBeforeCreatingVoc();

    tic = std::make_unique<MMMTokenIdConverter>();
    tic->initialize(jsonTokenizer, &config, verbose);
}

//------------------------------------------------------------------------
void MMM::loadNewConfigFile(const std::filesystem::path &tokenizerConfigFile)
{
    nlohmann::ordered_json jsonTokenizerConfig = libmusictokUtils::loadOrderedJson(tokenizerConfigFile);

    if (jsonTokenizerConfig.contains("config"))
    {
        config = TokenizerConfig::fromDict(jsonTokenizerConfig["config"]);
    }
    else
    {
        config = TokenizerConfig::fromDict(jsonTokenizerConfig);
    }

    tweakConfigBeforeCreatingVoc();
    tic = std::make_unique<MMMTokenIdConverter>();
    tic->initialize(jsonTokenizerConfig, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void MMM::tweakConfigBeforeCreatingVoc()
{
    config.usePrograms    = true;
    config.programChanges = true;
    // one_token_stream_for_programs is False so that the base_tokenizer treats each
    // track independently ((I,T) io) but one_token_stream True (set in __init__)
    // so that self (MMM)
    // has a (T) io as it will concatenate the tracks token sequences.
    config.oneTokenStreamForPrograms = false;

    if (config.additionalParams.find("base_tokenizer") == config.additionalParams.end())
    {
        std::ostringstream oss;
        oss << "MMM must be used with a `base_tokenizer`. This argument must be set in `config.additional_params` and "
               "reference to one of {";

        for (std::string tokenizerStr : mmmCompatibleTokenizers_default)
        {
            oss << tokenizerStr << ", ";
        }
        oss << "}.";
        std::cerr << oss.str() << std::endl;
    }

    // Add Track_Start and Track_End tokens to config
    for (const std::string &token : {"Track_Start", "Track_End"})
    {
        if (std::find(config.specialTokens.begin(), config.specialTokens.end(), token) == config.specialTokens.end())
        {
            config.specialTokens.push_back(token);
        }
    }

    TokenizerConfig baseTokenizerConfig = config;

    // Instantiate base tokenizer based on the base_tokenizer flag.
    if (config.additionalParams["base_tokenizer"] == "REMI")
    {
        baseTokenizer = std::make_unique<REMI>(baseTokenizerConfig);
    }
    else if (config.additionalParams["base_tokenizer"] == "TSD")
    {
        baseTokenizer = std::make_unique<TSD>(baseTokenizerConfig);
    }
    else if (config.additionalParams["base_tokenizer"] == "MIDILike")
    {
        baseTokenizer = std::make_unique<MIDILike>(baseTokenizerConfig);
    }
    else
    {
        throw std::invalid_argument("Only REMI is currently available as a base tokenizer.");
    }

    // Set config attribute
    baseTokenizer->config.usePrograms = true;

    // Copy noteOnOff attribute
    noteOnOff = baseTokenizer->getNoteOnOff();
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MMM::encode(
    const std::filesystem::path &scorePath, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    ScoreType score = libmusictokUtils::loadScoreFromMidi(scorePath);
    return encode(score, encodeIds, noPreprocessScore, attributeControlsIndexes);
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MMM::encode(
    const ScoreType &scoreIn, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    std::variant<TokSequence, std::vector<TokSequence>> sequences =
        MusicTokenizer::encode(scoreIn, encodeIds, noPreprocessScore, attributeControlsIndexes);

    assert(std::holds_alternative<TokSequenceVec>(sequences));
    TokSequenceVec tokSeqVec = std::get<TokSequenceVec>(sequences);
    TokSequence tokSeqVecSum;

    for (const TokSequence &seq : tokSeqVec)
    {
        tokSeqVecSum += seq;
    }
    return tokSeqVecSum;
}

//------------------------------------------------------------------------
std::variant<std::vector<TokSequence>, TokSequence> MMM::encodeWithoutConcatenation(
    const std::filesystem::path &scorePath, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    ScoreType score = libmusictokUtils::loadScoreFromMidi(scorePath);
    return encodeWithoutConcatenation(score, encodeIds, noPreprocessScore, attributeControlsIndexes);
}

//------------------------------------------------------------------------
std::variant<std::vector<TokSequence>, TokSequence> MMM::encodeWithoutConcatenation(
    const ScoreType &scoreIn, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    std::variant<TokSequence, std::vector<TokSequence>> sequences =
        MusicTokenizer::encode(scoreIn, encodeIds, noPreprocessScore, attributeControlsIndexes);

    assert(std::holds_alternative<TokSequenceVec>(sequences));
    TokSequenceVec tokSeqVec = std::get<TokSequenceVec>(sequences);
    return tokSeqVec;
}

//------------------------------------------------------------------------
std::variant<std::vector<Event>, std::vector<std::vector<Event>>> MMM::addTimeEvents(const std::vector<Event> &events,
                                                                                     symusic::i32 timeDivision) const
{
    std::vector<Event> trackEvents;
    if (events.empty())
    {
        return trackEvents;
    }

    if (events.size() < 2)
    {
        trackEvents = events;
    }
    else
    {
        if (baseTokenizer->type == TokenizerType::kREMI)
        {
            auto remi   = static_cast<REMI *>(baseTokenizer.get());
            trackEvents = std::get<std::vector<Event>>(remi->addTimeEvents(events, timeDivision));
        }
        else if (baseTokenizer->type == TokenizerType::kTSD)
        {
            auto tsd    = static_cast<TSD *>(baseTokenizer.get());
            trackEvents = std::get<std::vector<Event>>(tsd->addTimeEvents(events, timeDivision));
        }
        else if (baseTokenizer->type == TokenizerType::kMIDILike)
        {
            auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
            trackEvents   = std::get<std::vector<Event>>(midiLike->addTimeEvents(events, timeDivision));
        }
    }

    Event trackStartEvent = Event("Track", "Start", 0);
    Event trackEndEvent   = Event("Track", "End", events.back().time + 1);
    trackEvents.insert(trackEvents.begin(), trackStartEvent);
    trackEvents.insert(trackEvents.end(), trackEndEvent);
    return trackEvents;
}

//------------------------------------------------------------------------
void MMM::sortEvents(std::vector<Event> &events) const
{
    if (baseTokenizer->type == TokenizerType::kREMI)
    {
        auto remi = static_cast<REMI *>(baseTokenizer.get());
        remi->sortEvents(events);
    }
    else if (baseTokenizer->type == TokenizerType::kTSD)
    {
        auto tsd = static_cast<TSD *>(baseTokenizer.get());
        tsd->sortEvents(events);
    }
    else if (baseTokenizer->type == TokenizerType::kMIDILike)
    {
        auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
        midiLike->sortEvents(events);
    }
}

//------------------------------------------------------------------------
std::vector<TokSequence> MMM::splitTokSeqPerTrack(const TokSequence &tokSeq, bool keepTrackTokens) const
{
    //    return tic->splitTokSeqPerTrack(tokSeq, keepTrackTokens);
    return static_cast<MMMTokenIdConverter *>(tic.get())->splitTokSeqPerTrack(tokSeq, keepTrackTokens);
}

//------------------------------------------------------------------------
ScoreType MMM::tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                             const std::vector<std::tuple<int, bool>> &programs) const
{
    TokSequence tokseq     = std::get<TokSequence>(tokens);
    TokSequenceVec tokseqs = splitTokSeqPerTrack(tokseq);
    ScoreType scoreOut;

    if (baseTokenizer->type == TokenizerType::kREMI)
    {
        auto remi = static_cast<REMI *>(baseTokenizer.get());
        scoreOut  = remi->tokensToScore(tokseqs);
    }
    else if (baseTokenizer->type == TokenizerType::kTSD)
    {
        auto tsd = static_cast<TSD *>(baseTokenizer.get());
        scoreOut = tsd->tokensToScore(tokseqs);
    }
    else if (baseTokenizer->type == TokenizerType::kMIDILike)
    {
        auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
        scoreOut      = midiLike->tokensToScore(tokseqs);
    }

    return scoreOut;
}

//------------------------------------------------------------------------
std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> MMM::createBaseVocabulary()
{
    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> baseVocab;

    if (baseTokenizer->type == TokenizerType::kREMI)
    {
        auto remi = static_cast<REMI *>(baseTokenizer.get());
        baseVocab = remi->createBaseVocabulary();
    }
    else if (baseTokenizer->type == TokenizerType::kTSD)
    {
        auto tsd  = static_cast<TSD *>(baseTokenizer.get());
        baseVocab = tsd->createBaseVocabulary();
    }
    else if (baseTokenizer->type == TokenizerType::kMIDILike)
    {
        auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
        baseVocab     = midiLike->createBaseVocabulary();
    }

    assert(std::holds_alternative<std::vector<std::string>>(baseVocab));
    return baseVocab;
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::set<std::string>> MMM::createTokensTypesGraph()
{
    std::unordered_map<std::string, std::set<std::string>> ttg;

    if (baseTokenizer->type == TokenizerType::kREMI)
    {
        auto remi = static_cast<REMI *>(baseTokenizer.get());
        ttg       = remi->tokensTypesGraph;
    }
    else if (baseTokenizer->type == TokenizerType::kTSD)
    {
        auto tsd = static_cast<TSD *>(baseTokenizer.get());
        ttg      = tsd->tokensTypesGraph;
    }
    else if (baseTokenizer->type == TokenizerType::kMIDILike)
    {
        auto midiLike = static_cast<MIDILike *>(baseTokenizer.get());
        ttg           = midiLike->tokensTypesGraph;
    }

    return ttg;
}

//------------------------------------------------------------------------
int MMM::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    int err  = 0;
    size_t i = 0;
    while (i < tokens.size())
    {
        if (tokens.get()[i] != "Track_Start")
        {
            ++i;
            continue;
        }

        size_t j = i;
        while (j < tokens.size() && tokens.get()[j] != "Track_End")
        {
            ++j;
        }

        if (j > i + 1)
        {
            std::vector<std::string> subTokens(tokens.begin() + i + 1, tokens.begin() + j);
            err += baseTokenizer->tokensErrorsInternal(TokSeqAPI::Tokens(subTokens));
        }
        i = j;
    }
    return err;
}

//------------------------------------------------------------------------
} // namespace libmusictok
