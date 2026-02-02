//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : source/tokenizations/music_tokenizer.cpp
// Created by  : Steinberg, 02/2025
// Description : Music tokenizer Base class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/music_tokenizer.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <symusic/score.h>

namespace libmusictok {

//------------------------------------------------------------------------
// PUBLIC METHODS
//------------------------------------------------------------------------
MusicTokenizer::MusicTokenizer(TokenizerType type, bool verbose)
    : type{type}
    , verbose{verbose}
{
}

//------------------------------------------------------------------------
void MusicTokenizer::loadNewConfigFile(const std::filesystem::path &tokenizerConfigFile)
{
    const nlohmann::ordered_json &jsonTokenizerConfig = libmusictokUtils::loadOrderedJson(tokenizerConfigFile);

    if (jsonTokenizerConfig.contains("config"))
    {
        config = TokenizerConfig::fromDict(jsonTokenizerConfig["config"]);
    }
    else
    {
        config = TokenizerConfig::fromDict(jsonTokenizerConfig);
    }

    tweakConfigBeforeCreatingVoc();
    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(jsonTokenizerConfig, &config, verbose);
    updateTokenizer();
}

//------------------------------------------------------------------------
void MusicTokenizer::addToVocab(const Event &token, bool specialToken, int vocabIdx, std::u32string byte)
{
    this->addToVocab(token.str(), specialToken, vocabIdx, byte);
}

//------------------------------------------------------------------------
void MusicTokenizer::addToVocab(const std::string &token, bool specialToken, int vocabIdx, std::u32string byte)
{
    tic->addToVocab(token, specialToken, vocabIdx, byte);
}

//------------------------------------------------------------------------
bool MusicTokenizer::scoreHasTimeSignaturesNotInVocab(const ScoreType &score) const
{
    if (config.useTimeSignatures)
    {
        std::pair<int, int> timeSigPair;
        for (const TimeSigType &timeSig : *score.time_signatures)
        {
            timeSigPair = {timeSig.numerator, timeSig.denominator};
            if (std::find(timeSignatures.begin(), timeSignatures.end(), timeSigPair) != timeSignatures.end())
            {
                return true;
            }
        }
    }
    return false;
}

//------------------------------------------------------------------------
void MusicTokenizer::encodeTokenIds(TokSequence &sequence) const
{
    tic->encodeTokenIds(sequence);
}

//------------------------------------------------------------------------
void MusicTokenizer::encodeTokenIds(TokSequenceVec &sequence) const
{
    tic->encodeTokenIds(sequence);
}

//------------------------------------------------------------------------
void MusicTokenizer::decodeTokenIds(TokSequence &sequence) const
{
    tic->decodeTokenIds(sequence);
}

//------------------------------------------------------------------------
void MusicTokenizer::decodeTokenIds(TokSequenceVec &sequence) const
{
    tic->decodeTokenIds(sequence);
}

//------------------------------------------------------------------------
void MusicTokenizer::addAttributeControl(std::shared_ptr<AttributeControl> attributeControl_)
{
    this->attributeControls.push_back(attributeControl_);
    for (const auto &token : attributeControl_->tokens)
    {
        // method already checks to see if token is in vocab
        // addToVocab method is not fully implemented yet.
        tic->addToVocab(token);
    }
}

//------------------------------------------------------------------------
int MusicTokenizer::getPadToken() const
{
    if (isMultiVoc())
    {
        return tic->getItem("PAD_None", 0);
    }

    else
    {
        return tic->getItem("PAD_None");
    }
}

//------------------------------------------------------------------------
bool MusicTokenizer::isMultiVoc() const
{
    return this->tic->isMultiVoc();
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MusicTokenizer::encode(
    const std::filesystem::path &scorePath, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    const ScoreType &score = libmusictokUtils::loadScoreFromMidi(scorePath);
    return encode(score, encodeIds, noPreprocessScore, attributeControlsIndexes);
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MusicTokenizer::encode(
    const ScoreType &scoreIn, bool encodeIds, bool noPreprocessScore,
    AttributeControlsIndexesType attributeControlsIndexes) const
{
    ScoreType score;

    if (!noPreprocessScore)
    {
        score = preprocessScore(scoreIn);
    }
    else
    {
        score = scoreIn;
    }

    // ATTRIBUTE CONTROLS CURRENTLY UNSUPPORTED
    std::variant<TokSequence, std::vector<TokSequence>> tokens = scoreToTokens(score, attributeControlsIndexes);

    if (std::holds_alternative<TokSequence>(tokens))
    {
        libmusictokUtils::addBarBeatsTicksToTokseq(std::get<TokSequence>(tokens), score);
    }
    else if (std::holds_alternative<TokSequenceVec>(tokens))
    {
        libmusictokUtils::addBarBeatsTicksToTokseq(std::get<TokSequenceVec>(tokens), score);
    }

    if (encodeIds && tic->getIsTrained())
    {
        if (std::holds_alternative<TokSequence>(tokens))
        {
            tic->encodeTokenIds(std::get<TokSequence>(tokens));
        }
        if (std::holds_alternative<TokSequenceVec>(tokens))
        {
            tic->encodeTokenIds(std::get<TokSequenceVec>(tokens));
        }
    }

    return tokens;
}

//------------------------------------------------------------------------
void MusicTokenizer::complete_sequence(TokSequence &seq, bool complete_bytes) const
{
    tic->completeSequence(seq, complete_bytes);
}

//------------------------------------------------------------------------
void MusicTokenizer::createControlsForPedals(ScoreType &score) const
{
    // Create controls for pedals
    // This is required so that they are saved when the Score is dumped, as symusic
    // will only write the control messages.
    if (config.useSustainPedals)
    {
        for (symusic::shared<TrackType> track : *score.tracks)
        {
            for (const PedalType &pedals : *track->pedals)
            {
                track->controls->insert(track->controls->end(), ControlChangeType(pedals.time, 64, 127));
                track->controls->insert(track->controls->end(), ControlChangeType(pedals.end(), 64, 0));
            }
            if (track->pedals->size() > 0)
            {
                std::stable_sort(
                    track->controls->begin(), track->controls->end(),
                    [](const ControlChangeType &a, const ControlChangeType &b) { return a.time < b.time; });
            }
        }
    }

    // Set default tempo and time signatures at tick 0 if not present
    if (score.tempos->empty())
    {
        score.tempos->insert(score.tempos->begin(), TempoType::from_qpm(0, this->defaultTempo));
    }
    if (score.tempos->front().time != 0)
    {
        score.tempos->insert(score.tempos->begin(), TempoType::from_qpm(0, this->defaultTempo));
    }
    if (score.time_signatures->empty())
    {
        score.time_signatures->insert(score.time_signatures->begin(),
                                      TimeSigType(0, timeSignature_default.first, timeSignature_default.second));
    }
    if (score.time_signatures->front().time != 0)
    {
        score.time_signatures->insert(score.time_signatures->begin(),
                                      TimeSigType(0, timeSignature_default.first, timeSignature_default.second));
    }
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::decode(TokSequence &tokens, const std::vector<std::tuple<int, bool>> &programs,
                                 const std::filesystem::path &outputPath) const
{
    preprocessTokseqBeforeDecoding(tokens);

    ScoreType score = tokensToScore(tokens, programs);

    createControlsForPedals(score);

    if (!outputPath.empty())
    {
        libmusictokUtils::saveMidiFromScore(score, outputPath);
    }
    return score;
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::decode(TokSequenceVec &tokens, const std::vector<std::tuple<int, bool>> &programs,
                                 const std::filesystem::path &outputPath) const
{
    for (TokSequence &tokseq : tokens)
    {
        preprocessTokseqBeforeDecoding(tokseq);
    }

    ScoreType score = tokensToScore(tokens, programs);

    createControlsForPedals(score);

    if (!outputPath.empty())
    {
        libmusictokUtils::saveMidiFromScore(score, outputPath);
    }
    return score;
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::decode(const std::vector<int> &tokens, const std::vector<std::tuple<int, bool>> &programs,
                                 const std::filesystem::path &outputPath) const
{
    TokSequence tokseq = convertSequenceToTokseq(tokens);
    return decode(tokseq, programs, outputPath);
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::decode(const std::vector<std::vector<int>> &tokens,
                                 const std::vector<std::tuple<int, bool>> &programs,
                                 const std::filesystem::path &outputPath) const
{
    TokSequenceVec tokseq = convertSequenceToTokseq(tokens);
    return decode(tokseq, programs, outputPath);
}

//------------------------------------------------------------------------
std::vector<int> MusicTokenizer::getTokenIdsOfType(const std::string &tokenType, int vocabId) const
{
    return tic->getTokenIdsOfType(tokenType, vocabId);
}

//------------------------------------------------------------------------
std::string MusicTokenizer::ioFormat() const
{
    std::string format = "";
    if (!this->oneTokenStream)
    {
        format += "I";
    }
    format += "T";
    if (isMultiVoc())
    {
        format += "C";
    }
    return format;
}

//------------------------------------------------------------------------
bool MusicTokenizer::isTrained() const
{
    return tic->getIsTrained();
}

//------------------------------------------------------------------------
std::variant<StringIntMapVec, StringIntMap> MusicTokenizer::getVocab() const
{
    return tic->getVocab();
}

//------------------------------------------------------------------------
size_t MusicTokenizer::getVocabSize() const
{
    return tic->getVocabSize();
}

//------------------------------------------------------------------------
std::vector<std::string> MusicTokenizer::getSpecialTokens() const
{
    return config.specialTokens;
}

//------------------------------------------------------------------------
std::vector<int> MusicTokenizer::getSpecialTokenIds() const
{
    const std::vector<std::string> &specialTokens = getSpecialTokens();

    std::vector<int> output;
    output.reserve(specialTokens.size());

    for (const auto &token : specialTokens)
    {
        output.push_back(tic->getItem(token));
    }
    return output;
}

//------------------------------------------------------------------------
bool MusicTokenizer::getNoteOnOff() const
{
    return this->noteOnOff;
}

//------------------------------------------------------------------------
float MusicTokenizer::tokensErrors(const std::vector<int> &tokens) const
{
    const TokSequence &tokSeq = convertSequenceToTokseq(tokens);
    return tokensErrors(tokSeq);
}

//------------------------------------------------------------------------
std::vector<float> MusicTokenizer::tokensErrors(const std::vector<std::vector<int>> &tokens) const
{
    const TokSequenceVec &tokSeqVec = convertSequenceToTokseq(tokens);
    return tokensErrors(tokSeqVec);
}

//------------------------------------------------------------------------
std::vector<float> MusicTokenizer::tokensErrors(const TokSequenceVec &tokens) const
{
    std::vector<float> errors;
    for (const TokSequence &tokSeq : tokens)
    {
        errors.push_back(tokensErrorsInternal(tokSeq.tokens));
    }
    return errors;
}

//------------------------------------------------------------------------
float MusicTokenizer::tokensErrors(TokSequence tokens) const
{
    if (tokens.size() == 0)
    {
        return 0.0f;
    }

    size_t numTokPredicted = tokens.size();
    if (isTrained())
    {
        tic->decodeTokenIds(tokens);
    }
    tic->completeSequence(tokens);

    return static_cast<float>(tokensErrorsInternal(tokens.tokens)) / static_cast<float>(numTokPredicted);
}

//------------------------------------------------------------------------
TokenizerType MusicTokenizer::getType() const
{
    return this->type;
}

//------------------------------------------------------------------------
double MusicTokenizer::getDefaultTempo() const
{
    return this->defaultTempo;
}

//------------------------------------------------------------------------
// PRIVATE METHODS
//------------------------------------------------------------------------

//------------------------------------------------------------------------
void MusicTokenizer::loadFromJson(const std::filesystem::path &tokenizerFile)
{
    const nlohmann::ordered_json &jsonTokenizer = libmusictokUtils::loadJson(tokenizerFile);

    if (jsonTokenizer.contains("config"))
    {
        config = TokenizerConfig::fromDict(jsonTokenizer["config"]);
    }
    else
    {
        config = TokenizerConfig::fromDict(jsonTokenizer);
    }
    // Tweak the tokenizer's configuration and / or attributes before creating the
    // vocabulary. This method is intended to be overridden by inheriting tokenizer
    // classes.
    tweakConfigBeforeCreatingVoc();

    tic = std::make_unique<TokenIdConverter>();
    tic->initialize(jsonTokenizer, &config, verbose);
}

//------------------------------------------------------------------------
void MusicTokenizer::updateTokenizer()
{
    // Determines whether the tokenizer will produce a single sequence of tokens for
    // all the tracks one token sequence per track. This is attribute is distinct
    // from `config.one_token_stream_for_programs` as they might not be equal (e.g.
    // MMM). `tokenizer.one_token_stream` is meant to be used for i/o format
    // purposes whereas `config.one_token_stream_for_programs` is used for Score
    // preprocessing (merging tracks or not) and tokenization (all tracks at once or
    // treated separately).
    oneTokenStream = config.usePrograms && config.oneTokenStreamForPrograms;

    // Time Signatures
    // Need to be set before creating duration values/tokens.
    if (config.useTimeSignatures == true)
    {
        this->timeSignatures = createTimeSignatures();
    }
    else
    {
        this->timeSignatures = {timeSignature_default};
    }

    // Creates the numbers of ticks/beats depending on the note values (time
    // signature denominator) of the tokenizer. The values are calculated depending
    // on the maximum number of positions per beat
    // (`self.config.max_num_pos_per_beat`) and the time signatures it supports.
    // The highest note value will ba assigned a tick/beat value equal to
    // `self.config.max_num_pos_per_beat`, the other note values will have a
    // ticks/beat based on this base, i.e. `self.config.max_num_pos_per_beat`
    // multiplied by the factor between each note value and the maximum note value.
    this->tpbPerTs = createTpbPerTs();

    // Default time division to use when decoding tokens to a ``symusic.Score``.
    // This is left as a class attribute and not a property as the config is not
    // intended to be modified after its creation. Ultimately, this could be
    // ensured by converting TokenizerConfig to a frozen dataclass.
    // It shouldn't be used in place of the real ticks/beat value, which depends on
    // the current time signature denominator. The only exception is for tokenizers
    // which does not support time signature, i.e. which only consider 4/4.
    // During preprocessing, the Score will be resampled to a new time division equal
    // to the number of ticks per beat of the lowest time signature denominator it
    // contains. This is done in order to resample as much as possible while keeping
    // most of the accuracy. Some sections might need to be resampled again, when
    // the time signature denominator will be higher (i.e. higher number of absolute
    // ticks per beat).
    // Related: https://github.com/Yikai-Liao/symusic/issues/10
    this->timeDivision = this->tpbPerTs[timeSignature_default.second];

    // Durations
    // Usages:
    // Duration: tpb --> np.array (ticks) to get the closest;
    // Duration/TimeShift/Rest: ticks + tpb --> token (str);
    // Duration/TimeShift/Rest: token + tpb --> ticks (int);
    this->durations        = createDurationTuples();
    this->tpbToTimeArray   = createTpbToTicksArray();
    this->tpbTokensToTicks = createTpbTokensToTicks();
    this->tpbTicksToTokens = createTpbTicksToTokens();

    // Rests
    if (config.useRests)
    {
        this->rests = createRests();
    }
    this->tpbToRestArray  = createTpbToTicksArray(true);
    this->tpbRestsToTicks = createTpbTokensToTicks(true);

    // Velocities
    // [1:] so that there is no velocity_0
    if (config.useVelocities)
    {
        // Step 1: Create a vector with numVelocities + 1 values, matching np.linspace(0, 127, numVelocities + 1)
        this->velocities.resize(config.numVelocities + 1);
        for (int i = 0; i <= config.numVelocities; ++i)
        {
            // Perform the same math: linearly spaced, rounded to nearest integer
            this->velocities[i] = static_cast<symusic::i8>(
                std::floor(0.0 + (127.0 - 0.0) * i / static_cast<double>(config.numVelocities)));
        }
        // Step 2: Remove the 0th element so there is no velocity_0
        this->velocities.erase(this->velocities.begin());
    }
    else
    {
        this->velocities.clear();
    }
    this->firstBeatRes = config.beatRes.begin()->second;

    for (const auto &[beatRange, res] : config.beatRes)
    {
        if (beatRange.first <= 0 && 0 <= beatRange.second - 1)
        {
            // 0 in the range [first, second)
            this->firstBeatRes = res;
            break;
        }
    }

    // Tempo
    // _DEFAULT_TEMPO is the closest one to 120 that the tokenizer supports
    this->tempos.assign(1, 0.0);
    this->temposMspq.assign(1, 0.0);
    this->defaultTempo = tempo_default;

    if (config.useTempos)
    {
        this->tempos     = createTempos();
        this->temposMspq = libmusictokUtils::tempoQpmToMspq(tempos);

        std::stable_sort(this->temposMspq.begin(), this->temposMspq.end());

        // Find the value in tempos closest to TEMPO (tempo_default)
        auto it = std::min_element(this->tempos.begin(), this->tempos.end(), [](double a, double b) {
            return std::abs(a - tempo_default) < std::abs(b - tempo_default);
        });
        if (it != this->tempos.end())
        {
            this->defaultTempo = *it;
        }
    }

    // Pitch bends
    this->pitchBends.assign(1, 0);
    if (config.usePitchBends)
    {
        this->pitchBends = createPitchBends();
    }

    if (!this->tic->getIsVocabLoaded())
    {
        if (verbose)
            std::cout << "Creating vocabulary" << std::endl;
        createVocabulary();
    }

    // Attribute Controls
    if (config.oneTokenStreamForPrograms || this->isMultiVoc())
    {
        disableAttributeControls();
        if (verbose)
            std::clog << "Attribute controls are not compatible with 'config.one_token_stream_for_programs' and "
                         "multi-vocabulary tokenizers. Disabling them from the config."
                      << std::endl;
    }
    else
    {
        initialiseAttributeControls();
    }

    this->tokensTypesGraph = createTokensTypesGraph();
    addSpecialTokensToTypesGraph();
    tic->updateTokenTypesIndexes();
}

//------------------------------------------------------------------------
void MusicTokenizer::disableAttributeControls()
{
    // To be called in `_tweak_config_before_creating_voc` by tokenizers classes
    this->config.acPolyphonyTrack    = false;
    this->config.acPolyphonyBar      = false;
    this->config.acPitchClassBar     = false;
    this->config.acNoteDensityTrack  = false;
    this->config.acNoteDensityBar    = false;
    this->config.acNoteDurationBar   = false;
    this->config.acNoteDurationTrack = false;
    this->config.acRepetitionTrack   = false;
}

//------------------------------------------------------------------------
void MusicTokenizer::initialiseAttributeControls()
{
    if (config.acPolyphonyTrack)
        addAttributeControl(std::make_shared<TrackOnsetPolyphony>(config.acPolyphonyMin, config.acPolyphonyMax));

    if (config.acPolyphonyBar)
        addAttributeControl(std::make_shared<BarOnsetPolyphony>(config.acPolyphonyMin, config.acPolyphonyMax));

    if (config.acPitchClassBar)
        addAttributeControl(std::make_shared<BarPitchClass>());

    if (config.acNoteDensityTrack)
        addAttributeControl(
            std::make_shared<TrackNoteDensity>(config.acNoteDensityTrackMin, config.acNoteDensityTrackMax));

    if (config.acNoteDensityBar)
        addAttributeControl(std::make_shared<BarNoteDensity>(config.acNoteDensityBarMax));

    if (config.acNoteDurationBar)
        addAttributeControl(std::make_shared<BarNoteDuration>());

    if (config.acNoteDurationTrack)
        addAttributeControl(std::make_shared<TrackNoteDuration>());

    if (config.acRepetitionTrack)
        addAttributeControl(std::make_shared<TrackRepetition>(
            config.acRepetitionTrackNumBins, config.acRepetitionTrackNumConsecBars, config.pitchRange));
}

//------------------------------------------------------------------------
void MusicTokenizer::createVocabulary()
{
    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> tempVocab = createBaseVocabulary();
    tic->createVocabulary(tempVocab);
}

//------------------------------------------------------------------------
void MusicTokenizer::addSpecialTokensToTypesGraph()
{
    std::set<std::string> originalTokenTypes;
    for (const auto &kv : this->tokensTypesGraph)
    {
        originalTokenTypes.insert(kv.first);
    }

    for (const std::string &specialToken : this->config.specialTokens)
    {
        std::string specialTokenType = libmusictokUtils::split(specialToken, "_")[0];
        if (specialTokenType == eosTokenName_default)
        {
            tokensTypesGraph[eosTokenName_default] = {};
        }
        else
        {
            // union of originalTokenTypes and set of special tokens
            std::set<std::string> newTypes = originalTokenTypes;
            newTypes.insert(this->config.specialTokens.begin(), this->config.specialTokens.end());
            tokensTypesGraph[specialTokenType] = newTypes;
        }

        if (specialTokenType != bosTokenName_default)
        {
            for (const std::string &tokenType : originalTokenTypes)
            {
                tokensTypesGraph[tokenType].insert(specialTokenType);
            }
        }
    }
}

//------------------------------------------------------------------------
std::unordered_map<int, int> MusicTokenizer::createTpbPerTs() const
{
    // Find max denominator from timeSignatures
    int maxDenom = 0;
    for (const auto &ts : timeSignatures)
    {
        if (ts.second > maxDenom)
        {
            maxDenom = ts.second;
        }
    }

    std::unordered_map<int, int> tpbPerTs_;
    for (const auto &tsr : config.timeSignatureRange)
    {
        int denom        = tsr.first; // key of the map is the denominator
        tpbPerTs_[denom] = config.maxNumPosPerBeat() * (maxDenom / denom);
    }
    return tpbPerTs_;
}

//------------------------------------------------------------------------
std::vector<std::tuple<int, int, int>> MusicTokenizer::createDurationTuples()
{
    std::vector<std::tuple<int, int, int>> durations;

    // Iterate through each beat range and its associated beat resolution
    for (const auto &[beatRange, beatRes] : config.beatRes)
    {
        int beatStart = beatRange.first;
        int beatEnd   = beatRange.second;
        for (int beat = beatStart; beat < beatEnd; ++beat)
        {
            for (int pos = 0; pos < beatRes; ++pos)
            {
                durations.emplace_back(beat, pos, beatRes);
            }
        }
    }

    // Add the last one
    if (!config.beatRes.empty())
    {
        // Find the max key (beatRange) and associated beatRes
        auto maxIt   = std::max_element(config.beatRes.begin(), config.beatRes.end(),
                                        [](const auto &a, const auto &b) { return a.first.second < b.first.second; });
        int lastBeat = maxIt->first.second;
        int lastRes  = maxIt->second;
        durations.emplace_back(lastBeat, 0, lastRes);
    }

    // Remove duration of 0 (the first one)
    if (!durations.empty())
        durations.erase(durations.begin());

    return durations;
}

//------------------------------------------------------------------------
std::vector<std::tuple<int, int, int>> MusicTokenizer::createRests() const
{
    std::vector<std::tuple<int, int, int>> rests;

    // Iterate through each beat range and its associated beat resolution
    for (const auto &[beatRange, beatRes] : config.beatResRest)
    {
        int beatStart = beatRange.first;
        int beatEnd   = beatRange.second;
        for (int beat = beatStart; beat < beatEnd; ++beat)
        {
            for (int pos = 0; pos < beatRes; ++pos)
            {
                rests.emplace_back(beat, pos, beatRes);
            }
        }
    }

    // Add the last one
    if (!config.beatRes.empty())
    {
        // Find the max key (beatRange) and associated beatRes
        auto maxIt   = std::max_element(config.beatResRest.begin(), config.beatResRest.end(),
                                        [](const auto &a, const auto &b) { return a.first.second < b.first.second; });
        int lastBeat = maxIt->first.second;
        int lastRes  = maxIt->second;
        rests.emplace_back(lastBeat, 0, lastRes);
    }

    // Remove duration of 0 (the first one)
    if (!rests.empty())
        rests.erase(rests.begin());

    return rests;
}

//------------------------------------------------------------------------
std::vector<double> MusicTokenizer::createTempos() const
{
    const auto &tempoRange = config.tempoRange;
    int numTempos          = config.numTempos;

    std::vector<double> tempos_;
    tempos_.reserve(numTempos);

    if (config.logTempos)
    {
        // Geometric space (log scale)
        double start    = tempoRange.first;
        double end      = tempoRange.second;
        double logStart = std::log10(start);
        double logEnd   = std::log10(end);
        double step     = (logEnd - logStart) / (numTempos - 1);
        for (int i = 0; i < numTempos; ++i)
        {
            double val = std::pow(10, logStart + step * i);
            // Round to 2 decimal places
            val = libmusictokUtils::bankersRound(val * 100.0) / 100.0;
            tempos_.push_back(val);
        }
    }
    else
    {
        // Linear space
        double start = tempoRange.first;
        double end   = tempoRange.second;
        if (numTempos == 1)
        {
            tempos_.push_back(libmusictokUtils::bankersRound(start * 100.0) / 100.0);
        }
        else
        {
            double step = (end - start) / (numTempos - 1);
            for (int i = 0; i < numTempos; ++i)
            {
                double val = start + step * i;
                val        = libmusictokUtils::bankersRound(val * 100.0) / 100.0;
                tempos_.push_back(val);
            }
        }
    }
    return tempos_;
}

//------------------------------------------------------------------------
std::vector<symusic::i32> MusicTokenizer::createPitchBends() const
{
    // config.pitchBendRange: std::tuple<int, int, int> (start, end, numSteps)
    int start    = std::get<0>(config.pitchBendRange);
    int end      = std::get<1>(config.pitchBendRange);
    int numSteps = std::get<2>(config.pitchBendRange);

    std::vector<symusic::i32> pitchBends;

    // Guard against invalid number of steps
    if (numSteps <= 1)
    {
        // If numSteps is 1, just use the start point.
        pitchBends.push_back(static_cast<symusic::i32>(start));
        return pitchBends;
    }

    // Compute the step interval such that both endpoints are included and the interval is integer division
    // E.g., for start=0, end=10, numSteps=6 results: [0, 2, 4, 6, 8, 10]
    double step = static_cast<double>(end - start) / static_cast<double>(numSteps - 1);

    for (int i = 0; i < numSteps; ++i)
    {
        // Using rounding to avoid accumulating floating point error
        int val = static_cast<int>(std::floor(start + i * step));
        pitchBends.push_back(static_cast<symusic::i32>(val));
    }

    return pitchBends;
}

//------------------------------------------------------------------------
std::unordered_map<int, std::vector<symusic::i32>> MusicTokenizer::createTpbToTicksArray(bool rest) const
{
    std::vector<std::tuple<int, int, int>> values = rest ? this->rests : this->durations;
    std::unordered_map<int, std::vector<symusic::i32>> outTicksArray;

    std::vector<int> tpbPerTsValues;

    for (const auto &kv : tpbPerTs)
    {
        std::vector<symusic::i32> ticks;
        for (const std::tuple<int, int, int> &timeTuple : values)
        {
            ticks.push_back(timeTokenToTicks(timeTuple, kv.second));
        }

        outTicksArray[kv.second] = ticks;
    }
    return outTicksArray;
}

//------------------------------------------------------------------------
int MusicTokenizer::timeTokenToTicks(const std::tuple<int, int, int> &tokenDuration, int ticksPerBeat) const
{
    int beat = std::get<0>(tokenDuration);
    int pos  = std::get<1>(tokenDuration);
    int res  = std::get<2>(tokenDuration);

    return (beat * res + pos) * ticksPerBeat / res;
}

//------------------------------------------------------------------------
std::pair<std::vector<std::tuple<int, int, int>>, std::vector<int>> MusicTokenizer::timeTicksToTokens(int duration,
                                                                                                      int ticksPerBeat,
                                                                                                      bool rest) const
{
    const std::vector<int> &timeBins = rest ? tpbToRestArray.at(ticksPerBeat) : tpbToTimeArray.at(ticksPerBeat);
    const std::vector<std::tuple<int, int, int>> &timeTokens = rest ? rests : durations;
    int minTime                                              = timeBins[0];

    std::vector<std::tuple<int, int, int>> values;
    std::vector<int> offsetTimes;

    while (duration >= minTime)
    {
        // Find index of largest timeBin <= duration
        int index = -1;
        for (int i = 0; i < static_cast<int>(timeBins.size()); ++i)
        {
            if (timeBins[i] <= duration)
            {
                if (index == -1 || timeBins[i] > timeBins[index])
                {
                    index = i;
                }
            }
        }
        values.push_back(timeTokens[index]);
        int valTicks = timeBins[index];
        duration -= valTicks;
        offsetTimes.push_back(valTicks);
    }

    return {values, offsetTimes};
}

//------------------------------------------------------------------------
std::unordered_map<int, std::unordered_map<std::string, int>> MusicTokenizer::createTpbTokensToTicks(bool rest) const
{
    std::vector<std::tuple<int, int, int>> values = rest ? this->rests : this->durations;
    std::unordered_map<int, std::unordered_map<std::string, int>> outTpbTokensToTicks;

    for (const auto &kv : tpbPerTs)
    {
        int tpb = kv.second;
        std::unordered_map<std::string, int> tokensToTicks;

        for (const auto &durationTuple : values)
        {
            std::vector<std::string> durationStrings = {std::to_string(std::get<0>(durationTuple)),
                                                        std::to_string(std::get<1>(durationTuple)),
                                                        std::to_string(std::get<2>(durationTuple))};

            const std::string &token = libmusictokUtils::join(durationStrings, ".");
            int ticks                = this->timeTokenToTicks(durationTuple, tpb);

            tokensToTicks[token] = ticks;
        }
        outTpbTokensToTicks[tpb] = tokensToTicks;
    }

    return outTpbTokensToTicks;
}

//------------------------------------------------------------------------
std::unordered_map<int, std::unordered_map<int, std::string>> MusicTokenizer::createTpbTicksToTokens() const
{
    std::unordered_map<int, std::unordered_map<int, std::string>> outTpbTicksToTokens;

    for (const auto &tpbPair : this->tpbTokensToTicks)
    {
        int tpb                                                   = tpbPair.first;
        const std::unordered_map<std::string, int> &tokensToTicks = tpbPair.second;
        std::unordered_map<int, std::string> ticksToTokens;

        for (const auto &kv : tokensToTicks)
        {
            const std::string &token = kv.first;
            int ticks                = kv.second;
            ticksToTokens[ticks]     = token;
        }

        outTpbTicksToTokens[tpb] = ticksToTokens;
    }

    return outTpbTicksToTokens;
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::preprocessScore(const ScoreType &score) const
{
    using namespace symusic;
    pyvec<TimeSignature<Tick>> timeSigsCopy = *score.time_signatures;
    int newTpq                              = 0;

    if (config.useTimeSignatures)
    {
        filterUnsupportedTimeSignatures(timeSigsCopy);
        if (timeSigsCopy.size() == 0 || timeSigsCopy[0].time != 0)
        {
            timeSigsCopy.insert(timeSigsCopy.begin(),
                                TimeSignature<Tick>(0, timeSignature_default.first, timeSignature_default.second));
        }
        // get maximum time signature denominator
        int maxTSDenom = 0;
        for (const TimeSignature<Tick> &ts : timeSigsCopy)
        {
            if (ts.denominator > maxTSDenom)
            {
                maxTSDenom = ts.denominator;
            }
        }
        newTpq = config.maxNumPosPerBeat() * maxTSDenom / 4;
    }
    else // if not using timeSignatures
    {
        timeSigsCopy = pyvec<TimeSignature<Tick>>{
            TimeSignature<Tick>(0, timeSignature_default.first, timeSignature_default.second)};
        newTpq = config.maxNumPosPerBeat();
    }

    Score<Tick> scoreCopy = resampleScore(score, newTpq, timeSigsCopy);

    // Merge same program tracks if necessary
    if (config.usePrograms && config.oneTokenStreamForPrograms)
    {
        libmusictokUtils::mergeSameProgramTracks(scoreCopy.tracks);
    }

    // Redo timeSigsCopy, now from resampled score
    timeSigsCopy = *scoreCopy.time_signatures;
    if (config.useTimeSignatures && timeSigsCopy.size() > 0)
    {
        preprocessTimeSignatures(*scoreCopy.time_signatures, scoreCopy.ticks_per_quarter);
    }

    // Compute ticksPerBeat and tpqResamplingFactors
    std::vector<std::pair<int, int>> ticksPerBeat;
    std::vector<std::pair<int, int>> tpqResamplingFactors;
    bool computeTicksPerBeat = !noteOnOff || (config.useSustainPedals && config.sustainPedalDuration);

    if (computeTicksPerBeat)
    {
        if (config.useTimeSignatures && scoreCopy.time_signatures->size() > 0)
        {
            ticksPerBeat = libmusictokUtils::getScoreTicksPerBeat(scoreCopy);
        }
        else
        {
            ticksPerBeat = {
                {static_cast<int>(scoreCopy.end()), scoreCopy.ticks_per_quarter}
            };
        }
    }
    else
    {
        ticksPerBeat = {};
    }

    std::set<int> denominators;
    for (const auto &ts : *scoreCopy.time_signatures)
    {
        denominators.insert(ts.denominator);
    }

    if (config.useTimeSignatures && denominators.size() > 1)
    {
        tpqResamplingFactors = getScoreResamplingFactor(scoreCopy);
    }
    else
    {
        tpqResamplingFactors = {};
    }

    // Preprocess track events
    for (int t = static_cast<int>(scoreCopy.tracks->size()) - 1; t >= 0; --t)
    {
        auto &trackPtr = (*scoreCopy.tracks)[t];
        int program    = trackPtr->is_drum ? -1 : trackPtr->program;
        if (libmusictokUtils::isTrackEmpty(*trackPtr, false, config.useSustainPedals, config.usePitchBends) ||
            (config.usePrograms && config.programs.count(program) == 0))
        {
            scoreCopy.tracks->erase(scoreCopy.tracks->begin() + t);
            continue;
        }
        // Preprocess notes
        if (trackPtr->notes && trackPtr->notes->size() > 0)
        {
            preprocessNotes(*trackPtr, tpqResamplingFactors, ticksPerBeat);
        }

        // Pitch bends
        if (config.usePitchBends && trackPtr->pitch_bends && trackPtr->pitch_bends->size() > 0)
        {
            trackPtr->pitch_bends = preprocessPitchBends(*trackPtr->pitch_bends, tpqResamplingFactors);
        }

        // Pedals
        if (config.useSustainPedals && trackPtr->pedals && trackPtr->pedals->size() > 0)
        {
            trackPtr->pedals = preprocessPedals(*trackPtr->pedals, tpqResamplingFactors, ticksPerBeat);
        }

        if (libmusictokUtils::isTrackEmpty(*trackPtr, false, config.useSustainPedals, config.usePitchBends))
        {
            scoreCopy.tracks->erase(scoreCopy.tracks->begin() + t);
            continue;
        }
    }

    // Process tempo changes
    if (config.useTempos)
    {
        scoreCopy.tempos = preprocessTempos(*scoreCopy.tempos, tpqResamplingFactors);
    }

    // Key signatures, markers, and lyrics are not changed here as they are not used by
    // MIDITok yet

    return scoreCopy;
}

//------------------------------------------------------------------------
void MusicTokenizer::filterUnsupportedTimeSignatures(symusic::pyvec<TimeSigType> &timeSignatures_) const
{
    for (int i = static_cast<int>(timeSignatures_.size()) - 1; i >= 0; --i)
    {
        auto &ts = timeSignatures_[i];
        std::pair<int, int> tsPair(ts.numerator, ts.denominator);

        if (std::find(timeSignatures.begin(), timeSignatures.end(), tsPair) == timeSignatures.end())
        {
            if (verbose)
            {
                std::ostringstream oss;
                oss << "The file contains a time signature (" << ts
                    << ") outside of those supported by the tokenizer (";
                // Print all supported signatures
                for (size_t j = 0; j < timeSignatures.size(); ++j)
                {
                    oss << "(" << timeSignatures[j].first << "," << timeSignatures[j].second << ")";
                    if (j < timeSignatures.size() - 1)
                        oss << ", ";
                }
                oss << "). You should either discard this file or support this time signature, "
                    << "or alternatively deleting it however if you are using a beat-based tokenizer (REMI), "
                    << "the bars will be incorrectly detected.";

                std::cerr << oss.str() << std::endl;
            }
            timeSignatures_.erase(timeSignatures_.begin() + i);
        }
    }
}

//------------------------------------------------------------------------
ScoreType MusicTokenizer::resampleScore(const ScoreType &score, int newTpq,
                                        symusic::pyvec<TimeSigType> &timeSignaturesCopy) const
{
    if (score.ticks_per_quarter != newTpq)
    {
        for (TimeSigType &ts : timeSignaturesCopy)
        {
            ts.time = ts.time * newTpq / score.ticks_per_quarter;
        }

        ScoreType resampledScore       = symusic::resample(score, newTpq, 1);
        resampledScore.time_signatures = std::make_shared<symusic::pyvec<TimeSigType>>(timeSignaturesCopy);
        return resampledScore;
    }
    else
    {
        // Make a copy of the score to avoid in-place modification
        ScoreType scoreCopy       = score;
        scoreCopy.time_signatures = std::make_shared<symusic::pyvec<TimeSigType>>(timeSignaturesCopy);
        return scoreCopy;
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::preprocessTimeSignatures(symusic::pyvec<TimeSigType> &timeSignatureVec,
                                              symusic::i32 &timeDivision_) const
{
    size_t i         = 0;
    int ticksPerBar  = libmusictokUtils::computeTicksPerBar(timeSignatureVec[0], timeDivision_);
    int previousTick = 0; // first time signature change is always at tick 0

    while (i < timeSignatureVec.size())
    {
        // 1. If it is identical to the previous one --> delete it
        if (i >= 1 && libmusictokUtils::areTimeSignaturesEqual(timeSignatureVec[i], timeSignatureVec[i - 1]))
        {
            timeSignatureVec.erase(timeSignatureVec.begin() + i);
            continue;
        }

        // 2. Delay the time of each TS to the beginning of the next bar
        if ((i >= 1) && (timeSignatureVec[i].time < timeSignatureVec[i - 1].time))
        {
            timeSignatureVec[i].time = timeSignatureVec[i - 1].time;
        }
        int diff       = timeSignatureVec[i].time - previousTick;
        int bar_offset = diff / ticksPerBar;
        int rest       = diff % ticksPerBar;
        if (rest > 0)
        {
            timeSignatureVec[i].time = previousTick + (bar_offset + 1) * ticksPerBar;
        }
        // Update values
        ticksPerBar  = libmusictokUtils::computeTicksPerBar(timeSignatureVec[i], timeDivision_);
        previousTick = timeSignatureVec[i].time;

        // 3. If it is at the same tick as the previous one, we delete the previous
        if ((i >= 1) && (timeSignatureVec[i].time == timeSignatureVec[i - 1].time))
        {
            timeSignatureVec.erase(timeSignatureVec.begin() + (i - 1));
            // Check the one previous the one deleted is not equal to the current one
            if ((i >= 2) && libmusictokUtils::areTimeSignaturesEqual(timeSignatureVec[i - 1], timeSignatureVec[i - 2]))
            {
                // If it is, we delete the current one (at i-1 now) and decrement i
                timeSignatureVec.erase(timeSignatureVec.begin() + (i - 1));
                ticksPerBar  = libmusictokUtils::computeTicksPerBar(timeSignatureVec[i - 2], timeDivision_);
                previousTick = timeSignatureVec[i - 2].time;
                i -= 1;
            }
            continue;
        }

        // 4. Pass to the next one
        ++i;
    }

    // Make sure there is at least one time signature at tick 0
    if (!timeSignatureVec.empty())
    {
        if (config.deleteEqualSuccessiveTimeSigChanges &&
            (timeSignatureVec[0]->numerator == timeSignature_default.first) &&
            (timeSignatureVec[0]->denominator == timeSignature_default.second))
        {
            timeSignatureVec[0].time = 0;
        }
        else if (timeSignatureVec[0].time != 0)
        {
            timeSignatureVec.insert(timeSignatureVec.begin(),
                                    TimeSigType(0, timeSignature_default.first, timeSignature_default.second));
        }
    }
    else
    {
        timeSignatureVec.insert(timeSignatureVec.begin(),
                                TimeSigType(0, timeSignature_default.first, timeSignature_default.second));
    }
}

//------------------------------------------------------------------------
std::vector<std::pair<int, int>> MusicTokenizer::createTimeSignatures() const
{
    const std::vector<std::pair<int, std::vector<int>>> timeSignatureRange = config.timeSignatureRange;

    std::vector<std::pair<int, int>> timeSignatures_temp;

    for (const auto &[beat_res, beats] : timeSignatureRange)
    {
        if (beat_res <= 0 || std::log2(beat_res) != std::floor(std::log2(beat_res)))
        {
            std::ostringstream oss;
            oss << "The beat resolution (" << beat_res << ") in time signature must be a power of 2.";
            throw std::invalid_argument(oss.str());
        }

        for (int num_beats : beats)
        {
            timeSignatures_temp.emplace_back(num_beats, beat_res);
        }
    }

    return timeSignatures_temp;
}

//------------------------------------------------------------------------
std::vector<std::pair<int, int>> MusicTokenizer::getScoreResamplingFactor(const ScoreType &score) const
{
    const auto &tsigs = *score.time_signatures;
    std::vector<std::pair<int, int>> resamplingFactors;

    const int n_ts = static_cast<int>(tsigs.size());
    if (n_ts == 0)
        return resamplingFactors;

    // For each time signature change except the last
    for (int tsi = 0; tsi < n_ts - 1; ++tsi)
    {
        const auto &tsi_cur  = tsigs[tsi];
        const auto &tsi_next = tsigs[tsi + 1];

        int end_tick = static_cast<int>(tsi_next.time);

        // Equivalent to the Python integer division logic
        int factor =
            static_cast<int>(score.ticks_per_quarter / (config.maxNumPosPerBeat() * (tsi_cur.denominator / 4.0)));

        resamplingFactors.push_back({end_tick, factor});
    }

    // Handles last time signature up to the end of the score
    int last_end         = static_cast<int>(score.end()) + 1;
    int last_denominator = tsigs.back().denominator;
    int last_factor =
        static_cast<int>(score.ticks_per_quarter / (config.maxNumPosPerBeat() * (last_denominator / 4.0)));
    resamplingFactors.push_back({last_end, last_factor});

    // Remove equal successive ones, merge intervals
    for (int i = static_cast<int>(resamplingFactors.size()) - 1; i > 0; --i)
    {
        if (resamplingFactors[i].second == resamplingFactors[i - 1].second)
        {
            resamplingFactors[i - 1].first = resamplingFactors[i].first;
            resamplingFactors.erase(resamplingFactors.begin() + i);
        }
    }

    return resamplingFactors;
}

//------------------------------------------------------------------------
void MusicTokenizer::preprocessNotes(TrackType &track, std::vector<std::pair<int, int>> resamplingFactors,
                                     const std::vector<std::pair<int, int>> &ticksPerBeat, int minDuration) const
{
    // Get pointer or reference to the note array (struct-of-arrays or vector)
    symusic::pyvec<NoteType> &noteList = *track.notes;
    if (noteList.empty())
    {
        return;
    }

    // Determine the pitch range
    std::pair<int, int> pitchRange =
        (track.is_drum && config.usePitchdrumTokens) ? config.drumsPitchRange : config.pitchRange;

    // Remove notes outside of pitch range
    std::vector<size_t> idxOutOfPitchRange;
    for (size_t i = 0; i < noteList.size(); ++i)
    {
        const NoteType &note = noteList[i];
        if (note.pitch < pitchRange.first || note.pitch >= pitchRange.second)
        {
            idxOutOfPitchRange.push_back(i);
        }
    }
    if (!idxOutOfPitchRange.empty())
    {
        symusic::pyvec<NoteType> filtered;
        filtered.reserve(noteList.size() - idxOutOfPitchRange.size());
        for (size_t i = 0, j = 0; i < noteList.size(); ++i)
        {
            if (j < idxOutOfPitchRange.size() && i == idxOutOfPitchRange[j])
            {
                ++j;
                continue;
            }
            filtered.push_back(noteList[i]);
        }
        noteList = std::move(filtered);
    }
    if (noteList.empty())
    {
        track.notes = std::make_shared<symusic::pyvec<NoteType>>();
        return;
    }

    // Compute new velocities
    if (config.useVelocities)
    {
        // np_get_closest(self.velocities, np.array(note_soa["velocity"]))
        for (NoteType &note : noteList)
        {
            note.velocity = libmusictokUtils::npGetClosest(this->velocities, note.velocity);
        }
    }

    // Adjust times if needed
    if (!resamplingFactors.empty())
    {
        std::vector<int> noteTimesList;
        for (size_t i = 0; i < noteList.size(); ++i)
        {
            noteTimesList.push_back(noteList[i].time);
        }

        resamplingFactors = convertResamplingRatiosTicksToIdx(resamplingFactors, noteTimesList);

        adjustTimeToTpb(noteTimesList, resamplingFactors);

        for (size_t i = 0; i < noteList.size(); ++i)
        {
            noteList[i].time = noteTimesList[i];
        }
    }

    // Resample duration values if in useNoteDurationPrograms
    int program = track.is_drum ? -1 : track.program;
    if (std::find(config.useNoteDurationPrograms.begin(), config.useNoteDurationPrograms.end(), program) !=
        config.useNoteDurationPrograms.end())
    {
        if (!noteOnOff && !ticksPerBeat.empty())
        {
            adjustDurations(noteList, ticksPerBeat);
        }
        else if (!resamplingFactors.empty())
        {
            std::vector<int> noteDurationList;
            for (size_t i = 0; i < noteList.size(); ++i)
            {
                noteDurationList.push_back(noteList[i].duration);
            }

            adjustTimeToTpb(noteDurationList, resamplingFactors, minDuration);
            for (size_t i = 0; i < noteList.size(); ++i)
            {
                noteList[i].duration = noteDurationList[i];
            }

            adjustOffsetSpanningAcrossTimeSig(noteList, resamplingFactors);
        }
    }

    // Sort and remove duplicated notes if required
    if (config.removeDuplicatedNotes)
    {
        std::stable_sort(noteList.begin(), noteList.end(), [](const NoteType &a, const NoteType &b) {
            if (a.time != b.time)
                return a.time < b.time;
            if (a.pitch != b.pitch)
                return a.pitch < b.pitch;
            if (a.duration != b.duration)
                return a.duration < b.duration;
            return a.velocity < b.velocity;
        });
        libmusictokUtils::removeDuplicatedNotes(noteList);
    }

    track.notes = std::make_shared<symusic::pyvec<NoteType>>(noteList);
}

//------------------------------------------------------------------------
std::vector<std::pair<int, int>> MusicTokenizer::convertResamplingRatiosTicksToIdx(
    const std::vector<std::pair<int, int>> &resamplingFactors, const std::vector<int> &timeArray) const
{
    int idxFirst    = 0;
    auto factorsIdx = resamplingFactors; // copy
    for (size_t rfIdx = 0; rfIdx < resamplingFactors.size(); ++rfIdx)
    {
        int idxLast         = -1;
        auto lastTickFactor = resamplingFactors[rfIdx];

        if (rfIdx + 1 == resamplingFactors.size())
        {
            idxLast = static_cast<int>(timeArray.size()) - 1;
        }
        else
        {
            // Find the first index in timeArray[idxFirst:] where timeArray[idx] >= lastTickFactor.first
            for (size_t idx = idxFirst; idx < timeArray.size(); ++idx)
            {
                if (timeArray[idx] >= lastTickFactor.first)
                {
                    idxLast = static_cast<int>(idx);
                    break;
                }
            }
            // If not found, use the last index
            if (idxLast == -1)
                idxLast = static_cast<int>(timeArray.size()) - 1;
        }

        factorsIdx[rfIdx].first = idxLast;
        idxFirst                = idxLast;
    }
    return factorsIdx;
}

//------------------------------------------------------------------------
void MusicTokenizer::adjustTimeToTpb(std::vector<int> &timeArray,
                                     const std::vector<std::pair<int, int>> &tpqResamplingFactor, int minDuration) const
{
    int idxFirst = 0;
    for (size_t rfIdx = 0; rfIdx < tpqResamplingFactor.size(); ++rfIdx)
    {
        int idxLast = tpqResamplingFactor[rfIdx].first;
        int factor  = tpqResamplingFactor[rfIdx].second;

        int sliceEnd;
        if (rfIdx == tpqResamplingFactor.size() - 1)
        {
            sliceEnd = static_cast<int>(timeArray.size());
        }
        else
        {
            sliceEnd = idxLast;
        }

        if (factor != 1)
        {
            for (int i = idxFirst; i < sliceEnd; ++i)
            {
                timeArray[i] =
                    static_cast<int>(libmusictokUtils::bankersRound(static_cast<double>(timeArray[i]) / factor)) *
                    factor;
            }

            if (minDuration != -1)
            {
                for (int i = idxFirst; i < sliceEnd; ++i)
                {
                    if (timeArray[i] == 0)
                    {
                        timeArray[i] = minDuration * factor;
                    }
                }
            }
        }
        idxFirst = idxLast;
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::adjustDurations(symusic::pyvec<NoteType> &notes,
                                     const std::vector<std::pair<int, int>> &ticksPerBeat) const
{
    // notesOrPedals must be sorted by time.
    if (notes.empty() || ticksPerBeat.empty())
        return;

    size_t durIdxFirst = 0;
    int tickLastEvent  = notes.back().time;

    for (size_t tpbIdx = 0; tpbIdx < ticksPerBeat.size(); ++tpbIdx)
    {
        int lastTickTpb = ticksPerBeat[tpbIdx].first;
        int tpb         = ticksPerBeat[tpbIdx].second;

        // Find durIdxLast: first note whose time >= lastTickTpb
        size_t durIdxLast;
        bool isLast = (tpbIdx + 1 == ticksPerBeat.size()) || (lastTickTpb > tickLastEvent);
        if (isLast)
        {
            durIdxLast = notes.size();
        }
        else
        {
            durIdxLast = durIdxFirst;
            while (durIdxLast < notes.size() && notes[durIdxLast].time < lastTickTpb)
            {
                ++durIdxLast;
            }
        }

        // Adjust durations for the notesOrPedals in this tpb segment
        const std::vector<symusic::i32> &durationVocab = this->tpbToTimeArray.at(tpb);
        for (size_t i = durIdxFirst; i < durIdxLast; ++i)
        {
            notes[i].duration = libmusictokUtils::npGetClosest(durationVocab, notes[i].duration);
        }

        durIdxFirst = durIdxLast;
        if (isLast)
            break;
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::adjustDurations(symusic::pyvec<PedalType> &pedals,
                                     const std::vector<std::pair<int, int>> &ticksPerBeat) const
{
    // notesOrPedals must be sorted by time.
    if (pedals.empty() || ticksPerBeat.empty())
        return;

    size_t durIdxFirst = 0;
    int tickLastEvent  = pedals.back().time;

    for (size_t tpbIdx = 0; tpbIdx < ticksPerBeat.size(); ++tpbIdx)
    {
        int lastTickTpb = ticksPerBeat[tpbIdx].first;
        int tpb         = ticksPerBeat[tpbIdx].second;

        // Find durIdxLast: first note whose time >= lastTickTpb
        size_t durIdxLast;
        bool isLast = (tpbIdx + 1 == ticksPerBeat.size()) || (lastTickTpb > tickLastEvent);
        if (isLast)
        {
            durIdxLast = pedals.size();
        }
        else
        {
            durIdxLast = durIdxFirst;
            while (durIdxLast < pedals.size() && pedals[durIdxLast].time < lastTickTpb)
            {
                ++durIdxLast;
            }
        }

        // Adjust durations for the notesOrPedals in this tpb segment
        const std::vector<symusic::i32> &durationVocab = this->tpbToTimeArray.at(tpb);
        for (size_t i = durIdxFirst; i < durIdxLast; ++i)
        {
            pedals[i].duration = libmusictokUtils::npGetClosest(durationVocab, pedals[i].duration);
        }

        durIdxFirst = durIdxLast;
        if (isLast)
            break;
    }
}

//------------------------------------------------------------------------
template <typename T>
void MusicTokenizer::adjustOffsetSpanningAcrossTimeSig(
    symusic::pyvec<T> &notesOrPedals, const std::vector<std::pair<int, int>> &tpqResamplingFactors) const
{
    std::vector<symusic::Tick::unit> endArr(notesOrPedals.size());
    for (size_t i = 0; i < notesOrPedals.size(); ++i)
    {
        endArr[i] = notesOrPedals[i].time + notesOrPedals[i].duration;
    }

    size_t idxFirst = 0;
    for (size_t idxFact = 0; idxFact < tpqResamplingFactors.size() - 1; ++idxFact)
    {
        int idxLast = tpqResamplingFactors[idxFact].first;

        // The start time that splits the current time-sig segment:
        symusic::Tick::unit nextSegTime = notesOrPedals[idxLast].time;

        std::vector<size_t> spanningDurationsIdx;
        for (size_t i = idxFirst; i < static_cast<size_t>(idxLast); ++i)
        {
            if (endArr[i] >= nextSegTime)
            {
                spanningDurationsIdx.push_back(i);
            }
        }

        for (size_t idxLocal : spanningDurationsIdx)
        {
            size_t idx = idxLocal;

            size_t argMax = idxFact;
            while (argMax < tpqResamplingFactors.size() && tpqResamplingFactors[argMax].first < notesOrPedals[idx].time)
            {
                ++argMax;
            }
            if (argMax >= tpqResamplingFactors.size())
                argMax = tpqResamplingFactors.size() - 1;

            double factorForIdx = static_cast<double>(tpqResamplingFactors[argMax].second);

            symusic::Tick::unit oldEnd = endArr[idx];
            symusic::Tick::unit newEnd = static_cast<symusic::Tick::unit>(
                libmusictokUtils::bankersRound(static_cast<double>(oldEnd) / factorForIdx) * factorForIdx);
            notesOrPedals[idx].duration = newEnd - notesOrPedals[idx].time;
            endArr[idx]                 = newEnd;
        }
        idxFirst = idxLast;
    }
}

//------------------------------------------------------------------------
symusic::shared<symusic::pyvec<PitchBendType>> MusicTokenizer::preprocessPitchBends(
    const symusic::pyvec<PitchBendType> &bends, const std::vector<std::pair<int, int>> &tpqResamplingFactors) const
{
    const size_t n = bends.size();
    std::vector<int> pbTimes(n);
    std::vector<symusic::i32> pbValues(n);

    for (size_t i = 0; i < n; ++i)
    {
        pbTimes[i]  = bends[i].time;
        pbValues[i] = bends[i].value;
    }

    pbValues = libmusictokUtils::npGetClosest(this->pitchBends, pbValues);

    // Adjust times if required
    if (!tpqResamplingFactors.empty())
    {
        adjustTimeToTpb(pbTimes, tpqResamplingFactors);
    }

    // Deduplicate by onset (tick): keep largest absolute value at each tick
    if (n > 1)
    {
        // Group indices by (consecutive) same times
        std::vector<size_t> groupStartIndices;
        groupStartIndices.push_back(0);
        for (size_t i = 1; i < n; ++i)
        {
            if (pbTimes[i] != pbTimes[i - 1])
            {
                groupStartIndices.push_back(i);
            }
        }
        groupStartIndices.push_back(n);

        // For deletion, track indices to keep
        std::vector<size_t> indicesToKeep;
        for (size_t gi = 0; gi < groupStartIndices.size() - 1; ++gi)
        {
            size_t start = groupStartIndices[gi];
            size_t end   = groupStartIndices[gi + 1];
            if (end - start == 1)
            {
                indicesToKeep.push_back(start);
            }
            else
            {
                // Find max-abs value in group
                int maxAbs    = 0;
                size_t idxMax = start;
                for (size_t idx = start; idx < end; ++idx)
                {
                    if (std::abs(pbValues[idx]) > std::abs(pbValues[idxMax]))
                    {
                        idxMax = idx;
                    }
                }
                indicesToKeep.push_back(idxMax);
            }
        }

        // Make new result arrays
        std::vector<int> newTimes;
        std::vector<symusic::i32> newValues;
        newTimes.reserve(indicesToKeep.size());
        newValues.reserve(indicesToKeep.size());
        for (size_t idx : indicesToKeep)
        {
            newTimes.push_back(pbTimes[idx]);
            newValues.push_back(pbValues[idx]);
        }
        pbTimes  = std::move(newTimes);
        pbValues = std::move(newValues);
    }

    // Rebuild result pyvec
    auto result = std::make_shared<symusic::pyvec<PitchBendType>>();
    result->reserve(pbTimes.size());
    for (size_t i = 0; i < pbTimes.size(); ++i)
    {
        PitchBendType pb;
        pb.time  = pbTimes[i];
        pb.value = pbValues[i];
        result->push_back(pb);
    }
    return result;
}

//------------------------------------------------------------------------
symusic::shared<symusic::pyvec<PedalType>> MusicTokenizer::preprocessPedals(
    const symusic::pyvec<PedalType> &pedals, const std::vector<std::pair<int, int>> &tpqResamplingFactors,
    const std::vector<std::pair<int, int>> &ticksPerBeat, int minDuration) const
{
    size_t n = pedals.size();
    std::vector<int> pTimes(n);
    std::vector<int> pDurations(n);

    // Extract times and durations
    for (size_t i = 0; i < n; ++i)
    {
        pTimes[i]     = pedals[i].time;
        pDurations[i] = pedals[i].duration;
    }

    // Adjust times if needed
    if (!tpqResamplingFactors.empty())
    {
        std::vector<std::pair<int, int>> resamplingFactors_ =
            convertResamplingRatiosTicksToIdx(tpqResamplingFactors, pTimes);
        adjustTimeToTpb(pTimes, resamplingFactors_);
    }

    // Calculate end times
    std::vector<int> pEndTimes(n);
    for (size_t i = 0; i < n; ++i)
    {
        pEndTimes[i] = pTimes[i] + pDurations[i];
    }

    // Find overlapping pedals
    std::vector<bool> overlappingMask(n > 0 ? n - 1 : 0);
    std::vector<size_t> idxOverlap;

    for (size_t i = 0; i + 1 < n; ++i)
    {
        overlappingMask[i] = (pEndTimes[i] >= pTimes[i + 1]);
        if (overlappingMask[i])
        {
            idxOverlap.push_back(i);
        }
    }

    if (!idxOverlap.empty())
    {
        // Get first idx of each group - matching Python's diff logic
        std::vector<size_t> firstIdxGroups;
        firstIdxGroups.reserve(idxOverlap.size());

        firstIdxGroups.push_back(idxOverlap[0]);

        for (size_t k = 1; k < idxOverlap.size(); ++k)
        {
            size_t currentIdx = idxOverlap[k];
            size_t prevIdx    = idxOverlap[k - 1];

            if (currentIdx > prevIdx + 1)
            {
                firstIdxGroups.push_back(currentIdx);
            }
        }

        // Merge overlapping pedals
        std::vector<size_t> idxToDelete;
        for (size_t idx : firstIdxGroups)
        {
            size_t ip = 1;
            while ((idx + ip) < n && pEndTimes[idx] >= pTimes[idx + ip])
            {
                // Update duration to cover the overlap
                pDurations[idx] = std::max(pDurations[idx], pEndTimes[idx + ip] - pTimes[idx]);
                pEndTimes[idx]  = pTimes[idx] + pDurations[idx];
                idxToDelete.push_back(idx + ip);
                ip++;
            }
        }

        // Create mask for deduplication
        std::vector<bool> mask(n, true);
        for (size_t idx : idxToDelete)
        {
            if (idx < n)
                mask[idx] = false;
        }

        // Filter arrays using mask
        auto filterArray = [&mask, n](std::vector<int> &arr) {
            size_t writeIdx = 0;
            for (size_t i = 0; i < n; ++i)
            {
                if (mask[i])
                {
                    arr[writeIdx++] = arr[i];
                }
            }
            arr.resize(writeIdx);
        };

        filterArray(pTimes);
        filterArray(pDurations);
        n = pTimes.size();
    }

    // Create new pedals with filtered data
    symusic::pyvec<PedalType> newPedals;
    newPedals.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        newPedals.push_back(PedalType(pTimes[i], pDurations[i]));
    }

    // Handle duration adjustments
    if (config.sustainPedalDuration && !ticksPerBeat.empty())
    {
        adjustDurations(newPedals, ticksPerBeat);
    }
    else if (!tpqResamplingFactors.empty())
    {
        // Apply duration adjustments to the actual pedal objects
        auto resamplingFactors_ = convertResamplingRatiosTicksToIdx(tpqResamplingFactors, pTimes);

        // Extract current durations, adjust them, then update pedals
        std::vector<int> durationsToAdjust(n);
        for (size_t i = 0; i < n; ++i)
        {
            durationsToAdjust[i] = newPedals[i].duration;
        }

        adjustTimeToTpb(durationsToAdjust, resamplingFactors_, minDuration);

        // Update pedals with adjusted durations
        for (size_t i = 0; i < n; ++i)
        {
            newPedals[i].duration = durationsToAdjust[i];
        }

        adjustOffsetSpanningAcrossTimeSig(newPedals, resamplingFactors_);
    }

    return std::make_shared<symusic::pyvec<PedalType>>(newPedals);
}

//------------------------------------------------------------------------
symusic::shared<symusic::pyvec<TempoType>> MusicTokenizer::preprocessTempos(
    symusic::pyvec<TempoType> &tempos_, const std::vector<std::pair<int, int>> &tpqResamplingFactors) const
{
    // If input is empty, insert a default tempo at time 0 and return
    if (tempos_.size() == 0)
    {
        tempos_.append(TempoType::from_qpm(0, this->defaultTempo));
        return std::make_shared<symusic::pyvec<TempoType>>(tempos_);
    }

    // SoA (struct of arrays) preparation
    std::vector<double> mspqVec;
    std::vector<symusic::i32> timeVec;
    mspqVec.reserve(tempos_.size());
    timeVec.reserve(tempos_.size());
    for (const auto &tempo : tempos_)
    {
        mspqVec.push_back(tempo.mspq);
        timeVec.push_back(tempo.time);
    }

    // Snap mspq to the closest available values using npGetClosest
    mspqVec = libmusictokUtils::npGetClosest(this->temposMspq, mspqVec);

    // If resampling needed, adjust time accordingly
    if (!tpqResamplingFactors.empty())
    {
        adjustTimeToTpb(timeVec, tpqResamplingFactors);
    }

    // Compose back as SoA containers for editing by index
    struct TempoSoA
    {
        std::vector<symusic::i32> time;
        std::vector<double> mspq;
    } soa{timeVec, mspqVec};

    // Remove consecutive duplicate ticks: keep only last tempo per tick
    {
        std::vector<size_t> indices(soa.time.size());
        std::iota(indices.begin(), indices.end(), 0);

        std::vector<size_t> toDelete;

        for (size_t i = 1; i < soa.time.size(); ++i)
        {
            if (soa.time[i] == soa.time[i - 1])
            {
                toDelete.push_back(i - 1);
            }
        }

        for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it)
        {
            soa.time.erase(soa.time.begin() + *it);
            soa.mspq.erase(soa.mspq.begin() + *it);
        }
    }

    // Optionally deduplicate consecutive equal mspq values
    if (config.deleteEqualSuccessiveTempoChanges)
    {
        std::vector<size_t> toDelete;
        for (size_t i = 1; i < soa.mspq.size(); ++i)
        {
            if (soa.mspq[i] == soa.mspq[i - 1])
            {
                toDelete.push_back(i); // Remove all but first in equal block
            }
        }
        for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it)
        {
            soa.time.erase(soa.time.begin() + *it);
            soa.mspq.erase(soa.mspq.begin() + *it);
        }
    }

    // Reconstruct the pyvec<Tempo<Tick>>
    symusic::pyvec<TempoType> newTempos;
    for (size_t i = 0; i < soa.time.size(); ++i)
    {
        newTempos.append(TempoType(soa.time[i], soa.mspq[i]));
    }

    // Ensure at least one tempo at tick 0
    if (newTempos.size() > 0)
    {
        if (config.deleteEqualSuccessiveTempoChanges && TempoType::mspq2qpm(newTempos[0].mspq) == this->defaultTempo)
        {
            newTempos[0].time = 0;
        }
        else if (newTempos[0].time != 0)
        {
            newTempos.insert(0, TempoType::from_qpm(0, this->defaultTempo));
        }
    }
    else
    {
        newTempos.append(TempoType::from_qpm(0, this->defaultTempo));
    }

    return std::make_shared<symusic::pyvec<TempoType>>(newTempos);
}

//------------------------------------------------------------------------
std::variant<TokSequence, std::vector<TokSequence>> MusicTokenizer::scoreToTokens(
    const ScoreType &score, const AttributeControlsIndexesType &attributeControlIndexes) const
{
    std::vector<std::vector<Event>> allEventsVecVec;
    std::vector<Event> allEventsVec;
    std::vector<std::vector<Event>> allEventsVecMultiVoc;
    std::vector<std::vector<std::vector<Event>>> allEventsVecVecMultiVoc;

    if (!config.oneTokenStreamForPrograms)
    {
        if (score.tracks->size() == 0)
        {
            allEventsVecVec = {{}};
        }
        else
        {
            for (size_t i = 0; i < score.tracks->size(); ++i)
            {
                allEventsVecVec.push_back({});
            }
        }
    }

    std::vector<Event> globalEvents = createGlobalEvents(score);
    if (config.oneTokenStreamForPrograms)
    {
        allEventsVec.insert(allEventsVec.end(), globalEvents.begin(), globalEvents.end());
    }
    else
    {
        for (size_t i = 0; i < allEventsVecVec.size(); ++i)
        {
            allEventsVecVec[i].insert(allEventsVecVec[i].end(), globalEvents.begin(), globalEvents.end());
        }
    }

    // Compute ticks_per_beat sections depending on the time signatures
    // This has to be computed several times, in preprocess after resampling & here.
    std::vector<std::pair<int, int>> ticksPerBeat;
    if (!noteOnOff || (config.useSustainPedals && config.sustainPedalDuration) || config.useChords ||
        config.usePitchIntervals)
    {
        if (config.useTimeSignatures && score.time_signatures->size() > 0)
        {
            ticksPerBeat = libmusictokUtils::getScoreTicksPerBeat(score);
        }
        else
        {
            ticksPerBeat = {
                {score.end(), this->timeDivision}
            };
        }
    }
    else
    {
        ticksPerBeat = {};
    }

    // Adds track tokens
    std::vector<int> ticksBars  = libmusictokUtils::getBarsTicks(score, /*onlyNotesOnsets=*/true);
    std::vector<int> ticksBeats = libmusictokUtils::getBeatsTicks(score, /*onlyNotesOnsets=*/true);

    std::vector<Event> trackEvents;
    for (size_t i = 0; i < score.tracks->size(); ++i)
    {
        std::unordered_map<int, std::variant<std::vector<int>, bool>> trackAttributeControls;
        if (!attributeControlIndexes.empty())
        {
            trackAttributeControls = attributeControlIndexes.at(static_cast<int>(i));
        }
        else
        {
            trackAttributeControls = {};
        }
        trackEvents = createTrackEvents(*(*score.tracks)[i], ticksPerBeat, score.ticks_per_quarter, ticksBars,
                                        ticksBeats, trackAttributeControls);
        if (config.oneTokenStreamForPrograms)
        {
            allEventsVec.insert(allEventsVec.end(), trackEvents.begin(), trackEvents.end());
        }
        else
        {
            allEventsVecVec[i].insert(allEventsVecVec[i].end(), trackEvents.begin(), trackEvents.end());
            sortEvents(allEventsVecVec[i]);
        }
    }
    if (config.oneTokenStreamForPrograms)
    {
        sortEvents(allEventsVec);
        if (config.programChanges)
        {
            insertProgramChangeEvents(allEventsVec);
        }
    }
    else if (score.tracks->size() == 0 && allEventsVecVec[0].size() > 2)
    {
        sortEvents(allEventsVecVec[0]);
    }

    // Add time events
    if (config.oneTokenStreamForPrograms)
    {
        TokSequence tokSequence;
        if (!isMultiVoc())
        {
            allEventsVec       = std::get<std::vector<Event>>(addTimeEvents(allEventsVec, score.ticks_per_quarter));
            tokSequence.events = allEventsVec;
        }
        else
        {
            tokSequence.events =
                std::get<std::vector<std::vector<Event>>>(addTimeEvents(allEventsVec, score.ticks_per_quarter));
        }
        tic->completeSequence(tokSequence);
        return tokSequence;
    }
    else
    {
        std::vector<TokSequence> tokSequenceVec;
        for (size_t i = 0; i < allEventsVecVec.size(); ++i)
        {
            if (!isMultiVoc())
                allEventsVecVec[i] =
                    std::get<std::vector<Event>>(addTimeEvents(allEventsVecVec[i], score.ticks_per_quarter));
            else
                allEventsVecVecMultiVoc.push_back(std::get<std::vector<std::vector<Event>>>(
                    addTimeEvents(allEventsVecVec[i], score.ticks_per_quarter)));

            // Add program tokens at the beginning of the sequences if using
            // "program_changes" and not in "one_token_stream" mode.
            if (config.programChanges && score.tracks->size() > 0)
            {
                int program = (*(*score.tracks)[i]).is_drum ? -1 : static_cast<int>((*(*score.tracks)[i]).program);

                // If the first token is a "Track_Start" the program token is
                // inserted after
                allEventsVecVec[i].insert((allEventsVecVec[i][0].type != "Track") ? allEventsVecVec[i].begin()
                                                                                  : allEventsVecVec[i].begin() + 1,
                                          Event("Program", program, 0));
            }
            if (!isMultiVoc())
            {
                TokSequence tokSeq;
                tokSeq.events = allEventsVecVec[i];
                tic->completeSequence(tokSeq);
                tokSequenceVec.push_back(tokSeq);
            }
            else
            {
                TokSequence tokSeq;
                tokSeq.events = allEventsVecVecMultiVoc[i];
                tokSeq.tokens = std::vector<std::vector<std::string>>();
                tic->completeSequence(tokSeq);
                tokSequenceVec.push_back(tokSeq);
            }
        }

        return tokSequenceVec;
    }
}

//------------------------------------------------------------------------
std::vector<Event> MusicTokenizer::createGlobalEvents(const ScoreType &score) const
{
    std::vector<Event> events;

    // First adds time signature tokens if specified
    if (config.useTimeSignatures)
    {
        for (const auto &timeSig : *score.time_signatures)
        {
            events.push_back(Event("TimeSig",
                                   std::to_string(timeSig.numerator) + "/" + std::to_string(timeSig.denominator),
                                   timeSig.time));
        }
    }

    // Adds tempo events if specified
    if (config.useTempos)
    {
        for (const auto &tempo : *score.tempos)
        {
            events.push_back(Event("Tempo", libmusictokUtils::roundToTwoDecimals((tempo.qpm() * 100) / 100.0),
                                   tempo.time, 0, std::to_string(tempo.qpm())));
        }
    }

    return events;
}

//------------------------------------------------------------------------
std::vector<Event> MusicTokenizer::createTrackEvents(
    const TrackType &track, const std::vector<std::pair<int, int>> &ticksPerBeat, symusic::i32 timeDivision,
    const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
    const std::unordered_map<int, std::variant<std::vector<int>, bool>> &attributeControlIndexes) const
{
    int program       = track.is_drum ? -1 : track.program;
    bool useDurations = config.useNoteDurationPrograms.count(program) > 0;
    std::vector<Event> events;

    // For pitch intervals
    int maxTimeInterval = 0;
    if (config.usePitchIntervals && !ticksPerBeat.empty())
    {
        maxTimeInterval = ticksPerBeat[0].second * config.pitchIntervalsMaxTimeDist;
    }
    int previousNoteOnset  = -maxTimeInterval - 1;
    int previousPitchOnset = -128;
    int previousPitchChord = -128;

    // Attribute controls
    if (!attributeControlIndexes.empty())
    {
        for (const auto &[acIdx, trackBarsIdx] : attributeControlIndexes)
        {
            BarAttributeControl *bacPtr = static_cast<BarAttributeControl *>(attributeControls[acIdx].get());
            // this checks if attributeControls[acIdx] is a BarAttributeControl
            if (bacPtr != nullptr && std::holds_alternative<std::vector<int>>(trackBarsIdx) &&
                std::get<std::vector<int>>(trackBarsIdx).empty())
                continue;

            auto result = attributeControls[acIdx]->compute(track, timeDivision, ticksBars, ticksBeats,
                                                            std::get<std::vector<int>>(trackBarsIdx));
            events.insert(events.end(), result.begin(), result.end());
        }
    }

    // Pedals
    if (config.useSustainPedals)
    {
        size_t tpbIdx = 0;
        for (const auto &pedal : *track.pedals)
        {
            events.emplace_back("Pedal", program, pedal.time, program);
            if (config.sustainPedalDuration && !ticksPerBeat.empty())
            {
                while (pedal.time >= ticksPerBeat[tpbIdx].first)
                {
                    ++tpbIdx;
                }
                std::string dur = tpbTicksToTokens.at(ticksPerBeat.at(tpbIdx).second).at(pedal.duration);
                events.emplace_back("Duration", dur, pedal.time, program, "PedalDuration");
            }
            else
            {
                events.emplace_back("PedalOff", program, pedal.end(), program);
            }
        }
    }

    // Pitch bends
    if (config.usePitchBends)
    {
        for (const auto &pb : *track.pitch_bends)
        {
            if (config.usePrograms && !config.programChanges)
            {
                events.emplace_back("Program", program, pb.time, program, "ProgramPitchBend");
            }
            events.emplace_back("PitchBend", pb.value, pb.time, program);
        }
    }

    // Chords
    if (config.useChords && !track.is_drum)
    {
        // ticksPerBeat has to have a value if useChords is true, but good to make sure
        assert(!ticksPerBeat.empty());
        auto chords =
            libmusictokUtils::detectChords(track.notes, ticksPerBeat, config.chordMaps, program,
                                           config.chordTokensWithRootNote, firstBeatRes, 1, config.chordUnknown);
        for (const auto &chord : chords)
        {
            if (config.usePrograms && !config.programChanges)
            {
                events.emplace_back("Program", program, chord.time, program, "ProgramChord");
            }
            events.push_back(chord);
        }
    }

    // Notes
    size_t tpbIdx = 0;
    for (const auto &note : *track.notes)
    {
        // Program event at note start
        if (config.usePrograms && !config.programChanges)
        {
            events.emplace_back("Program", program, note.start(), program, note.end());
        }
        // Pitch Intervals
        bool addAbsolutePitchToken = true;
        if (config.usePitchIntervals && !track.is_drum)
        {
            // Adjust maxTimeInterval if needed
            if (!ticksPerBeat.empty() && note.time >= ticksPerBeat[tpbIdx].first)
            {
                ++tpbIdx;
                maxTimeInterval = ticksPerBeat[tpbIdx].second * config.pitchIntervalsMaxTimeDist;
            }
            if (note.start() != previousNoteOnset)
            {
                if (note.start() - previousNoteOnset <= maxTimeInterval &&
                    abs(note.pitch - previousPitchOnset) <= config.maxPitchInterval)
                {
                    events.emplace_back("PitchIntervalTime", note.pitch - previousPitchOnset, note.start(), program,
                                        note.end());
                    addAbsolutePitchToken = false;
                }
                previousPitchOnset = previousPitchChord = note.pitch;
            }
            else
            {
                if (abs(note.pitch - previousPitchChord) <= config.maxPitchInterval)
                {
                    events.emplace_back("PitchIntervalChord", note.pitch - previousPitchChord, note.start(), program,
                                        note.end());
                    addAbsolutePitchToken = false;
                }
                else
                {
                    previousPitchOnset = note.pitch;
                }
                previousPitchChord = note.pitch;
            }
            previousNoteOnset = note.start();
        }
        // Pitch/NoteOn
        if (addAbsolutePitchToken)
        {
            std::string n = (config.usePitchdrumTokens && track.is_drum) ? (this->noteOnOff ? "DrumOn" : "PitchDrum")
                                                                         : (this->noteOnOff ? "NoteOn" : "Pitch");
            events.emplace_back(n, note.pitch, note.start(), program, note.end());
        }
        // Velocity
        if (config.useVelocities)
        {
            events.emplace_back("Velocity", note.velocity, note.start(), program, std::to_string(note.velocity));
        }

        // Duration/NoteOff
        if (useDurations)
        {
            if (this->noteOnOff)
            {
                if (config.usePrograms && !config.programChanges)
                {
                    events.emplace_back("Program", program, note.end(), program, "ProgramNoteOff");
                }
                std::string offType = (config.usePitchdrumTokens && track.is_drum) ? "DrumOff" : "NoteOff";
                events.emplace_back(offType, note.pitch, note.end(), program, note.end());
            }
            else
            {
                events.push_back(createDurationEvent(note, program, ticksPerBeat, static_cast<int>(tpbIdx)));
            }
        }
    }
    return events;
}

//------------------------------------------------------------------------
Event MusicTokenizer::createDurationEvent(const NoteType &note, int program,
                                          const std::vector<std::pair<int, int>> &ticksPerBeat, int tpbIdx) const
{
    // Advance tpbIdx while note.time >= ticksPerBeat[tpbIdx][0]
    while (note.time >= ticksPerBeat[tpbIdx].first)
    {
        ++tpbIdx;
    }
    std::string dur = this->tpbTicksToTokens.at(ticksPerBeat.at(tpbIdx).second).at(note.duration);
    return Event("Duration", dur, note.start(), program, std::to_string(note.duration) + " ticks");
}

//------------------------------------------------------------------------
bool MusicTokenizer::startsWith(const std::string &str, const std::string &prefix) const
{
    return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin());
}

//------------------------------------------------------------------------
void MusicTokenizer::sortEvents(std::vector<Event> &events) const
{
    // Sort events by their timestamp
    std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.time < b.time; });

    // Set track-level attribute control event times to zero after sorting
    if (!this->attributeControls.empty())
    {
        for (auto &event : events)
        {
            if (!startsWith(event.type, "ACTrack"))
            {
                break;
            }
            event.time = 0;
        }
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::insertProgramChangeEvents(std::vector<Event> &events) const
{
    int previousProgram = -2;
    std::string previousType;
    std::vector<std::pair<size_t, Event>> programChangeEvents;

    for (size_t ei = 0; ei < events.size(); ++ei)
    {
        const auto &event      = events[ei];
        bool needProgramChange = event.program != previousProgram &&
                                 std::find(tokenTypeBeforePc_default.begin(), tokenTypeBeforePc_default.end(),
                                           event.type) == tokenTypeBeforePc_default.end() &&
                                 !(event.type == "Duration" && previousType == "Pedal") && event.type != "Pedal" &&
                                 event.type != "PedalOff";

        if (needProgramChange)
        {
            previousProgram = event.program;
            programChangeEvents.emplace_back(ei, Event("Program", previousProgram, event.time));
        }
        previousType = event.type;
    }

    for (auto it = programChangeEvents.rbegin(); it != programChangeEvents.rend(); ++it)
    {
        events.insert(events.begin() + it->first, it->second);
    }
}

//------------------------------------------------------------------------
bool MusicTokenizer::areIdsEncoded(const std::vector<int> &ids) const
{
    size_t vocabSize = tic->getBaseVocabSize();
    for (int id_ : ids)
    {
        if (id_ >= vocabSize)
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------
TokSequence MusicTokenizer::convertSequenceToTokseq(const std::vector<int> &idsVec) const
{
    TokSequence tokseq = TokSequence({}, idsVec);
    if (this->isTrained())
    {
        tokseq.areIdsEncoded = areIdsEncoded(idsVec);
    }

    return tokseq;
}

//------------------------------------------------------------------------
TokSequenceVec MusicTokenizer::convertSequenceToTokseq(const std::vector<std::vector<int>> &idsVecVec) const
{
    TokSequenceVec tokseqVec;
    for (const std::vector<int> &idsVec : idsVecVec)
    {
        tokseqVec.push_back(convertSequenceToTokseq(idsVec));
    }

    return tokseqVec;
}

//------------------------------------------------------------------------
TokSequence MusicTokenizer::convertSequenceToTokseq(const std::vector<std::string> &tokenStrVec) const
{
    return TokSequence(tokenStrVec);
}

//------------------------------------------------------------------------
TokSequenceVec MusicTokenizer::convertSequenceToTokseq(
    const std::vector<std::vector<std::string>> &tokenStrVecVec) const
{
    TokSequenceVec tokseqVec;
    for (const std::vector<std::string> &tokenStrVec : tokenStrVecVec)
    {
        tokseqVec.push_back(convertSequenceToTokseq(tokenStrVec));
    }

    return tokseqVec;
}

//------------------------------------------------------------------------
TokSequence MusicTokenizer::convertSequenceToTokseq(const std::vector<Event> &eventVec) const
{
    return TokSequence({}, {}, {}, eventVec);
}

//------------------------------------------------------------------------
TokSequenceVec MusicTokenizer::convertSequenceToTokseq(const std::vector<std::vector<Event>> &eventVecVec) const
{
    TokSequenceVec tokseqVec;
    for (const std::vector<Event> &eventVec : eventVecVec)
    {
        tokseqVec.push_back(convertSequenceToTokseq(eventVec));
    }

    return tokseqVec;
}

//------------------------------------------------------------------------
void MusicTokenizer::preprocessTokseqBeforeDecoding(TokSequence &tokSeq) const
{
    if (tokSeq.tokens.size() == 0)
    {
        if (tokSeq.areIdsEncoded)
        {
            tic->decodeTokenIds(tokSeq);
        }
        tic->completeSequence(tokSeq);
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::addNoteTokensToVocabList(std::vector<std::string> &vocab) const
{
    // NoteOn + NoteOff
    if (noteOnOff)
    {
        for (size_t i = config.pitchRange.first; i < config.pitchRange.second; ++i)
        {
            vocab.push_back("NoteOn_" + std::to_string(i));
        }
        if (config.useNoteDurationPrograms.size() > 0)
        {
            for (size_t i = config.pitchRange.first; i < config.pitchRange.second; ++i)
            {
                vocab.push_back("NoteOff_" + std::to_string(i));
            }
        }
    }
    else
    {
        for (size_t i = config.pitchRange.first; i < config.pitchRange.second; ++i)
        {
            vocab.push_back("Pitch_" + std::to_string(i));
        }
    }

    // Velocity
    if (config.useVelocities)
    {
        for (auto vel : this->velocities)
        {
            vocab.push_back("Velocity_" + std::to_string(vel));
        }
    }

    // Duration
    if ((!noteOnOff && config.usingNoteDurationTokens()) || config.sustainPedalDuration)
    {
        for (auto dur : this->durations)
        {
            vocab.push_back("Duration_" + libmusictokUtils::join(dur, "."));
        }
    }
}

//------------------------------------------------------------------------
void MusicTokenizer::addAdditionalTokensToVocabList(std::vector<std::string> &vocab) const
{
    // PitchInterval
    if (config.usePitchIntervals)
    {
        for (const std::string &intervalType : {"PitchIntervalTime_", "PitchIntervalChord_"})
        {
            for (int pitchIdx = -config.maxPitchInterval; pitchIdx < config.maxPitchInterval + 1; ++pitchIdx)
            {
                vocab.push_back(intervalType + std::to_string(pitchIdx));
            }
        }
    }

    // PitchDrum
    if (config.usePitchdrumTokens)
    {
        std::string pitchTokenName = noteOnOff ? "DrumOn" : "PitchDrum";
        for (int pitchBend = config.drumsPitchRange.first; pitchBend < config.drumsPitchRange.second; ++pitchBend)
        {
            vocab.push_back(pitchTokenName + "_" + std::to_string(pitchBend));
        }
        if (noteOnOff)
        {
            for (int pitchBend = config.drumsPitchRange.first; pitchBend < config.drumsPitchRange.second; ++pitchBend)
            {
                vocab.push_back("DrumOff_" + std::to_string(pitchBend));
            }
        }
    }

    // Chord
    if (config.useChords)
    {
        std::vector<std::string> chordsTokens = createChordsTokens();
        vocab.insert(vocab.end(), chordsTokens.begin(), chordsTokens.end());
    }

    // Rest
    if (config.useRests)
    {
        for (const auto &rest : rests)
        {
            vocab.push_back("Rest_" + libmusictokUtils::join(rest, "."));
        }
    }

    // Tempo
    if (config.useTempos)
    {
        for (const double &i : tempos)
        {
            vocab.push_back("Tempo_" + libmusictokUtils::roundToTwoDecimals(i));
        }
    }

    // Program
    if (config.usePrograms)
    {
        for (const auto &program : config.orderedPrograms)
        {
            vocab.push_back("Program_" + std::to_string(program));
        }
    }

    // TimeSig
    if (config.useTimeSignatures)
    {
        for (const auto &ts : timeSignatures)
        {
            vocab.push_back("TimeSig_" + std::to_string(ts.first) + "/" + std::to_string(ts.second));
        }
    }

    // Pedal
    if (config.useSustainPedals)
    {
        if (config.usePrograms)
        {
            for (const auto &program : config.programs)
            {
                vocab.push_back("Pedal_" + std::to_string(program));
                if (!config.sustainPedalDuration)
                {
                    vocab.push_back("PedalOff_" + std::to_string(program));
                }
            }
        }
        else
        {
            vocab.push_back("Pedal_0");
            if (!config.sustainPedalDuration)
            {
                vocab.push_back("PedalOff_0");
            }
        }
    }

    // PitchBend
    if (config.usePitchBends)
    {
        for (const auto &pitchBend : pitchBends)
        {
            vocab.push_back("PitchBend_" + std::to_string(pitchBend));
        }
    }
}

//------------------------------------------------------------------------
std::vector<std::string> MusicTokenizer::createChordsTokens() const
{
    std::vector<std::string> tokens;

    if (config.chordTokensWithRootNote)
    {
        for (const auto &chordQuality : config.chordMaps)
        {
            for (const auto &rootNote : pitchClasses_default)
            {
                tokens.push_back("Chord_" + rootNote + ":" + chordQuality.first);
            }
        }
    }
    else
    {
        for (const auto &chordQuality : config.chordMaps)
        {
            tokens.push_back("Chord_" + chordQuality.first);
        }
    }

    // Unknown chords
    if (!config.chordUnknown.empty())
    {
        std::vector<int> chordUnknown = config.chordUnknown;

        assert(chordUnknown.size() == 2);
        if (config.chordTokensWithRootNote)
        {
            for (int i = chordUnknown[0]; i < chordUnknown[1]; ++i)
            {
                for (const auto &rootNote : pitchClasses_default)
                {
                    tokens.push_back("Chord_" + rootNote + ":" + unknownChordPrefix_default + std::to_string(i));
                }
            }
        }
        else
        {
            for (int i = chordUnknown[0]; i < chordUnknown[1]; ++i)
            {
                tokens.push_back("Chord_" + std::to_string(i));
            }
        }
    }
    return tokens;
}

//------------------------------------------------------------------------
std::pair<int, int> MusicTokenizer::parseTokenTimeSignature(const std::string &tokenTimeSig)
{
    size_t slashPos = tokenTimeSig.find('/');
    if (slashPos == std::string::npos || slashPos == 0 || slashPos == tokenTimeSig.size() - 1)
    {
        throw std::invalid_argument("Invalid time signature format: " + tokenTimeSig);
    }
    int numerator   = std::stoi(tokenTimeSig.substr(0, slashPos));
    int denominator = std::stoi(tokenTimeSig.substr(slashPos + 1));
    return {numerator, denominator};
}

//------------------------------------------------------------------------
int MusicTokenizer::minRest(int ticksPerBeat) const
{
    if (!config.useRests)
    {
        return 0;
    }
    return this->tpbToRestArray.at(ticksPerBeat)[0];
}

//----------------------------------------------------------------------------
int MusicTokenizer::tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const
{
    if (tokens.empty())
        return 0;

    int errType              = 0;
    int errTime              = 0;
    int errNote              = 0;
    std::string previousType = libmusictokUtils::split(tokens.get()[0], "_")[0];
    int currentPos           = -1;
    int currentProgram       = 0;
    std::unordered_map<int, std::vector<int>> currentPitches;
    std::unordered_map<int, int> previousPitchOnset;
    std::unordered_map<int, int> previousPitchChord;
    std::vector<std::string> noteTokenTypes = {"Pitch", "NoteOn", "PitchDrum"};
    int pitchVal;

    for (int prog : config.programs)
    {
        currentPitches[prog]     = {};
        previousPitchOnset[prog] = -128;
        previousPitchChord[prog] = -128;
    }

    if (config.usePitchIntervals)
    {
        noteTokenTypes.push_back("PitchIntervalTime");
        noteTokenTypes.push_back("PitchIntervalChord");
    }

    // Init first note and current pitches if needed
    if (std::find(noteTokenTypes.begin(), noteTokenTypes.end(), previousType) != noteTokenTypes.end())
    {
        pitchVal = std::stoi(libmusictokUtils::split(tokens.get()[0], "_")[1]);
        currentPitches[currentProgram].push_back(pitchVal);
    }
    else if (previousType == "Position")
    {
        currentPos = std::stoi(libmusictokUtils::split(tokens.get()[0], "_")[1]);
    }

    for (size_t ti = 0; ti < tokens.size() - 1; ++ti)
    {
        const std::string token = tokens.get()[ti + 1];

        const std::string eventType  = libmusictokUtils::split(token, "_")[0];
        const std::string eventValue = libmusictokUtils::split(token, "_")[1];

        // Good token type
        if (std::find(tokensTypesGraph.at(previousType).begin(), tokensTypesGraph.at(previousType).end(), eventType) !=
            tokensTypesGraph.at(previousType).end())
        {
            if (token == "Bar_None") // reset
            {
                currentPos = -1;
                for (int prog : config.programs)
                {
                    currentPitches[prog] = {};
                }
            }
            else if (eventType == "TimeShift" || eventType == "Time-Shift" || eventType == "Rest")
            {
                for (int prog : config.programs)
                {
                    currentPitches[prog] = {};
                }
            }
            else if (std::find(noteTokenTypes.begin(), noteTokenTypes.end(), eventType) != noteTokenTypes.end())
            {
                if (eventType == "Pitch" || eventType == "NoteOn" || eventType == "PitchDrum")
                {
                    pitchVal                           = std::stoi(eventValue);
                    previousPitchOnset[currentProgram] = pitchVal;
                    previousPitchChord[currentProgram] = pitchVal;
                }
                else if (eventType == "PitchIntervalTime")
                {
                    pitchVal                           = previousPitchOnset[currentProgram] + std::stoi(eventValue);
                    previousPitchOnset[currentProgram] = pitchVal;
                    previousPitchChord[currentProgram] = pitchVal;
                }
                else
                {
                    pitchVal                           = previousPitchChord[currentProgram] + std::stoi(eventValue);
                    previousPitchChord[currentProgram] = pitchVal;
                }

                if (config.removeDuplicatedNotes &&
                    std::find(currentPitches[currentProgram].begin(), currentPitches[currentProgram].end(), pitchVal) !=
                        currentPitches[currentProgram].end())
                {
                    ++errNote;
                }
                else
                {
                    currentPitches[currentProgram].push_back(pitchVal);
                }
            }
            else if (eventType == "Position")
            {
                // With time signatures, if can happen that Rest -> TimeSig -> Position
                if (std::stoi(eventValue) <= currentPos && previousType != "Rest" &&
                    !(previousType == "TimeSig" && libmusictokUtils::split(tokens.get()[ti - 1], "_")[0] == "Rest"))
                {
                    ++errTime;
                }
                currentPos = std::stoi(eventValue);
                for (int prog : config.programs)
                {
                    currentPitches[prog] = {};
                }
            }
            else if (eventType == "Program")
            {
                currentProgram = std::stoi(eventValue);
            }
        }
        else // Bad token type
        {
            ++errType;
        }
        previousType = eventType;
    }
    return errType + errTime + errNote;
}

//------------------------------------------------------------------------
} // namespace libmusictok
