//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/token_id_converter.h
// Created by  : Steinberg, 02/2025
// Description : TokenIdConverter (private) class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/event.h"
#include "libmusictok/tok_sequence.h"
#include "tokenizer_config.h"
#include "tokenizers_cpp.h"
#include "utility_functions.h"

#include <cassert>
#include <codecvt>
#include <locale>
#include <string>
#include <variant>
#include <vector>

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
using vocabType = std::variant<std::vector<std::unordered_map<std::string, int>>, std::unordered_map<std::string, int>>;
using vocabInvType =
    std::variant<std::vector<std::unordered_map<int, std::string>>, std::unordered_map<int, std::string>>;

//------------------------------------------------------------------------
class TokenIdConverterBase
{
public:
    TokenIdConverterBase() {};
    virtual ~TokenIdConverterBase() {};

    // Initialiser
    void initialize(const nlohmann::json &musicTokenizerConfigs, TokenizerConfig *tokenizerConfig, bool verbose = true);

    // --- Loaders ---
    void loadModel(const std::string &modelParamsAsJsonString);

    void loadVocabBase(const nlohmann::json &vocabBaseJson);

    void loadVocabBaseBytes(const nlohmann::json &vocabBaseByteToTokenJson);

    // --- Encoding ---
    void encodeTokenIds(std::vector<TokSequence> &sequenceOfTokens) const;

    virtual void encodeTokenIds(TokSequence &sequenceOfTokens) const;

    // --- Decoding ---
    void decodeTokenIds(std::vector<TokSequence> &sequenceOfTokens) const;

    void decodeTokenIds(TokSequence &sequenceOfTokens) const;

    ///< Fills in the missing fields in the TokSequence object
    void completeSequence(TokSequence &sequenceOfTokens, bool completeBytes = false) const;

    ///< Creates the tokenizer vocabulary
    void createVocabulary(std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> &tempBaseVocab);

    ///< Adds a token to the vocabulary
    void addToVocab(const std::string &tokenAsString, bool isSpecialToken = false, int vocabIdx = -1,
                    const std::u32string &byte = std::u32string());

    ///< Returns the tokenizer vocabulary
    vocabType getVocab() const;

    ///< Recomputes the tokenTypesIndexes
    void updateTokenTypesIndexes();

    ///< Splits a TokSequence by track (For MMM tokenizer)
    virtual std::vector<TokSequence> splitTokSeqPerTrack(const TokSequence &tokSeq,
                                                         bool keepTrackTokens = false) const = 0;

    // --- GetItem ---

    ///< Returns the ID of a given Token string
    int getItem(const std::string &token) const;

    ///< Returns the ID of a given Token string and it's vocab Idx (if isMultiVoc)
    int getItem(const std::string &token, const int &vocabIdx) const;

    ///< Return the token string given an id
    std::string getItem(const int &id_) const;

    ///< Returns the token string given an id and a vocab idx (if isMultiVoc)
    std::string getItem(const int &id_, const int &vocabIdx) const;

    ///< Checks if the tokenizer has multiple vocabularies
    bool isMultiVoc() const;

    ///< Checks if the vocab was loaded
    bool getIsVocabLoaded();

    ///< Checks if the tokenizer is trained (by BPE, for example)
    bool getIsTrained();

    ///< Returns the vocab size
    size_t getVocabSize();

    ///< Returns the vocab size of the base (per-training) vocabulary
    size_t getBaseVocabSize();

    ///< Returns the IDs of a given type. vocabId = -1 for non-MultiVoc
    std::vector<int> getTokenIdsOfType(const std::string &tokenType, int vocabId = -1);

protected:
    std::vector<int> tokensToIds(const std::vector<std::string> &tokens) const;
    std::vector<std::vector<int>> tokensToIds(const std::vector<std::vector<std::string>> &tokens) const;
    TokSeqAPI::Ids tokensToIds(const TokSeqAPI::Tokens &tokens) const;

    std::vector<std::string> eventsToTokens(const std::vector<Event> &events) const;
    std::vector<std::vector<std::string>> eventsToTokens(const std::vector<std::vector<Event>> &events) const;
    TokSeqAPI::Tokens eventsToTokens(const TokSeqAPI::Events &tokens) const;

    // The following methods originally accepted an input asString (def. true) which, when false,
    // would return events instead. If that is your intention, please use idsToEvents and
    // bytesToEvents instead.
    std::vector<std::string> idsToTokens(const std::vector<int> &ids) const;
    std::vector<std::vector<std::string>> idsToTokens(const std::vector<std::vector<int>> &ids) const;
    TokSeqAPI::Tokens idsToTokens(const TokSeqAPI::Ids &ids) const;

    std::vector<Event> idsToEvents(const std::vector<int> &ids) const;
    std::vector<std::vector<Event>> idsToEvents(const std::vector<std::vector<int>> &ids) const;
    TokSeqAPI::Events idsToEvents(const TokSeqAPI::Ids &ids) const;

    std::vector<std::string> bytesToTokens(const std::u32string &bytes) const;
    std::vector<std::vector<std::string>> bytesToTokens(const std::vector<std::u32string> &bytes) const;
    TokSeqAPI::Tokens bytesToTokens(const TokSeqAPI::Bytes &bytes) const;

    std::vector<Event> bytesToEvents(const std::u32string &bytes) const;
    std::vector<std::vector<Event>> bytesToEvents(const std::vector<std::u32string> &bytes) const;
    TokSeqAPI::Events bytesToEvents(const TokSeqAPI::Bytes &bytes) const;

    // Originally, this method accepted asOneString (def. false) as an option,
    // which when true, returned a string as opposed to a list[str]. If you need
    // a list[str], please use bytesToEventVec
    std::u32string idsToBytes(const std::vector<int> &ids) const;
    std::vector<std::u32string> idsToBytes(const std::vector<std::vector<int>> &ids) const;
    TokSeqAPI::Bytes idsToBytes(const TokSeqAPI::Ids &ids) const;
    std::vector<std::u32string> idsToBytesVec(const std::vector<int> &ids) const;
    std::vector<std::vector<std::u32string>> idsToBytesVec(const std::vector<std::vector<int>> &ids) const;

    void createVocabLearnedBytesToTokens();
    void resetState();

    //------------------------------------------------------------------------
    // Member variables
    //------------------------------------------------------------------------
    // HFTokenizer
    std::unique_ptr<tokenizers::Tokenizer> model;

    // Configs
    TokenizerConfig *config;
    bool verbose;

    // Vocabulary
    vocabType vocabBase;
    vocabInvType vocabBaseInv;
    std::unordered_map<std::u32string, std::string> vocabBaseByteToToken;
    std::variant<std::vector<std::unordered_map<std::string, std::vector<int>>>,
                 std::unordered_map<std::string, std::vector<int>>>
        tokenTypesIndexes;

    std::unordered_map<std::u32string, std::vector<std::string>> vocabLearnedBytesToTokens;
    std::unordered_map<int, std::u32string> vocabBaseIdToByte;
    std::unordered_map<std::u32string, int> vocabModel;
    bool isTrained     = false;
    bool isVocabLoaded = false;

    std::u32string continuingSubwordPrefix;
    std::u32string endOfWordSuffix;
    std::string preTokenizerType;
    std::u32string preTokenizerReplacement;
};

//------------------------------------------------------------------------
class TokenIdConverter final : public TokenIdConverterBase
{
    std::vector<TokSequence> splitTokSeqPerTrack(const TokSequence &tokSeq,
                                                 bool keepTrackTokens = false) const override;
};

//------------------------------------------------------------------------
class MMMTokenIdConverter final : public TokenIdConverterBase
{
public:
    void encodeTokenIds(TokSequence &sequenceOfTokens) const override;
    std::vector<TokSequence> splitTokSeqPerTrack(const TokSequence &tokSeq,
                                                 bool keepTrackTokens = false) const override;
};

} // namespace libmusictok
