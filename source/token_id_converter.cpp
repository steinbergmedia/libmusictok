//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/token_id_converter.cpp
// Created by  : Steinberg, 02/2025
// Description : TokenIdConverter (private) class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/token_id_converter.h"
#include "libmusictok/constants.h"
#include "libmusictok/utility_functions.h"
#include <cassert>
#include <stdexcept>

namespace libmusictok {
// ---------------
// PUBLIC METHODS|
//------------------------------------------------------------------------
void TokenIdConverterBase::initialize(const nlohmann::json &musicTokenizerConfigs, TokenizerConfig *tokenizerConfig,
                                      bool v)
{
    resetState();
    this->verbose = v;

    if (musicTokenizerConfigs.contains("_model"))
    {
        std::string modelParams = musicTokenizerConfigs["_model"];
        loadModel(modelParams);
    }

    if (musicTokenizerConfigs.contains("_vocab_base"))
    {
        isVocabLoaded                = true;
        nlohmann::json vocabBaseJson = musicTokenizerConfigs["_vocab_base"];
        loadVocabBase(vocabBaseJson);
    }

    if (musicTokenizerConfigs.contains("_vocab_base_byte_to_token"))
    {
        nlohmann::json vocabBaseJson = musicTokenizerConfigs["_vocab_base_byte_to_token"];
        loadVocabBaseBytes(vocabBaseJson);
    }

    this->config = tokenizerConfig;
}

//------------------------------------------------------------------------
void TokenIdConverterBase::loadModel(const std::string &modelParamsAsJsonString)
{
    model                      = tokenizers::Tokenizer::FromBlobJSON(modelParamsAsJsonString);
    isTrained                  = true;
    nlohmann::json parsedModel = nlohmann::json::parse(modelParamsAsJsonString);
    std::string version        = parsedModel["version"];

    // Instead of directly converting to unordered_map<std::u32string, int>,
    // convert using std::string keys first.
    nlohmann::json vocabModelJson = parsedModel["model"]["vocab"];
    auto tempvocabModel           = vocabModelJson.get<std::unordered_map<std::string, int>>();

    // Convert std::string keys to std::u32string using your utility function
    for (const auto &entry : tempvocabModel)
    {
        vocabModel[libmusictokUtils::utf8_to_utf32(entry.first)] = entry.second;
    }

    auto tempEndOfWordSuffix         = parsedModel["model"]["end_of_word_suffix"].get<std::string>();
    auto tempContinuingSubwordPrefix = parsedModel["model"]["continuing_subword_prefix"].get<std::string>();

    endOfWordSuffix         = libmusictokUtils::utf8_to_utf32(tempEndOfWordSuffix);
    continuingSubwordPrefix = libmusictokUtils::utf8_to_utf32(tempContinuingSubwordPrefix);

    preTokenizerType = parsedModel["pre_tokenizer"]["type"].get<std::string>();

    if (preTokenizerType == "Metaspace")
    {
        auto tempPreTokenizerReplacement = parsedModel["pre_tokenizer"]["replacement"].get<std::string>();
        preTokenizerReplacement          = libmusictokUtils::utf8_to_utf32(tempPreTokenizerReplacement);
    }
}
//------------------------------------------------------------------------
void TokenIdConverterBase::loadVocabBase(const nlohmann::json &vocabBaseJson)
{
    //--------------------------------------------------------------------
    // We can assume that the vocabBase will always have the type below as
    // the multivoc tokenizers do not save the vocabs in the json, and instead
    // create them at intilisation.
    //--------------------------------------------------------------------
    auto vocabBase_ = vocabBaseJson.get<std::unordered_map<std::string, int>>();
    vocabBase       = vocabBase_;

    std::unordered_map<int, std::string> vocabBaseInv_;

    for (const auto &entry : vocabBase_)
    {
        vocabBaseInv_[entry.second] = entry.first;
    }

    vocabBaseInv = vocabBaseInv_;
}

//------------------------------------------------------------------------
void TokenIdConverterBase::loadVocabBaseBytes(const nlohmann::json &vocabBaseByteToTokenJson)
{
    //--------------------------------------------------------------------
    // We can assume that the vocabBase will always have the type below as
    // the multivoc tokenizers do not save the vocabs in the json, and instead
    // create them at intilisation.
    //--------------------------------------------------------------------
    auto vocabBase_ = std::get<std::unordered_map<std::string, int>>(vocabBase);

    auto tempVocabBaseByteToToken = vocabBaseByteToTokenJson.get<std::unordered_map<std::string, std::string>>();

    for (const auto &[key, value] : tempVocabBaseByteToToken)
    {
        // Convert the key from UTF-8 to UTF-16 and insert into the map.
        vocabBaseByteToToken[libmusictokUtils::utf8_to_utf32(key)] = value;
    }

    std::unordered_map<std::string, std::u32string> tokenToByte_;
    for (const auto &entry : vocabBaseByteToToken)
    {
        tokenToByte_[entry.second] = entry.first;
    }

    for (const auto &entry : vocabBase_)
    {
        vocabBaseIdToByte[entry.second] = tokenToByte_.at(entry.first);
    }

    // Automatically create this as it's only creatable after this point.
    createVocabLearnedBytesToTokens();
}

//------------------------------------------------------------------------
void TokenIdConverterBase::encodeTokenIds(std::vector<TokSequence> &sequenceOfTokens) const
{
    // recursively encore each sequenceOfTokens
    for (TokSequence &tokSeq : sequenceOfTokens)
    {
        encodeTokenIds(tokSeq);
    }
}

//------------------------------------------------------------------------
void TokenIdConverterBase::encodeTokenIds(TokSequence &sequenceOfTokens) const
{
    auto _splitSeqBytes = [this](TokSequence &seq__) -> std::vector<std::u32string> {
        // Declare a string to hold the resulting bytes.
        std::vector<std::u32string> result;

        if (this->config->encodeIdsSplit == "bar")
        {
            std::vector<TokSequence> parts = seq__.splitPerBars();
            for (const TokSequence &subseq : parts)
            {
                result.push_back(subseq.bytes.get());
            }
            return result;
        }

        if (this->config->encodeIdsSplit == "beat")
        {
            std::vector<TokSequence> parts = seq__.splitPerBeats();
            for (const TokSequence &subseq : parts)
            {
                result.push_back(subseq.bytes.get());
            }
            return result;
        }
        // Default: return the sequence bytes.
        result.push_back(seq__.bytes.get());
        return result;
    };

    this->completeSequence(sequenceOfTokens, true);

    // Split the sequence bytes into a single string.
    std::vector<std::u32string> allBytesUTF32 = _splitSeqBytes(sequenceOfTokens);

    //convert to utf8 for encode method:
    std::vector<std::string> allBytesUTF8;
    for (const std::u32string &byteStr32 : allBytesUTF32)
    {
        allBytesUTF8.push_back(libmusictokUtils::utf32_to_utf8(byteStr32));
    }

    std::vector<int32_t> encodedTokens;
    std::vector<std::vector<int32_t>> batchedEncodedTokens =
        this->model->EncodeBatch(allBytesUTF8 /* add_special_tokens=true*/);
    for (const std::vector<int32_t> &encodedTokenSplit : batchedEncodedTokens)
    {
        encodedTokens.insert(encodedTokens.end(), encodedTokenSplit.begin(), encodedTokenSplit.end());
    }
    sequenceOfTokens.ids           = encodedTokens;
    sequenceOfTokens.areIdsEncoded = true;
}

//------------------------------------------------------------------------
void TokenIdConverterBase::decodeTokenIds(std::vector<TokSequence> &sequenceOfTokens) const
{
    for (TokSequence &seq : sequenceOfTokens)
    {
        // Recursively decode each sequence.
        this->decodeTokenIds(seq);
    }
}

//------------------------------------------------------------------------
void TokenIdConverterBase::decodeTokenIds(TokSequence &sequenceOfTokens) const
{
    // Only decode if the ids are still encoded.
    if (sequenceOfTokens.areIdsEncoded)
    {
        // Create a vector to hold the final, decoded tokens.
        std::vector<std::string> decodedTokens;

        // For each id, get the corresponding "byte" (string) from the model.
        for (const int &id : sequenceOfTokens.ids)
        {
            // Retrieve the encoded byte from the model.
            std::string encodedByte                   = this->model->IdToToken(id);
            std::u32string encodedByte16              = libmusictokUtils::utf8_to_utf32(encodedByte);
            const std::vector<std::string> &tokenList = this->vocabLearnedBytesToTokens.at(encodedByte16);

            for (const std::string &token : tokenList)
            {
                // Append the tokens from tokenList to the flattened list.
                decodedTokens.push_back(token);
            }
        }

        sequenceOfTokens.tokens = decodedTokens;

        sequenceOfTokens.ids = this->tokensToIds(decodedTokens);

        sequenceOfTokens.areIdsEncoded = false;
    }
}

//------------------------------------------------------------------------
void TokenIdConverterBase::completeSequence(TokSequence &sequenceOfTokens, bool completeBytes) const
{
    if (sequenceOfTokens.tokens.size() == 0)
    {
        if (sequenceOfTokens.events.size() > 0)
        {
            sequenceOfTokens.tokens = this->eventsToTokens(sequenceOfTokens.events);
        }
        else if (sequenceOfTokens.ids.size() > 0)
        {
            sequenceOfTokens.tokens = this->idsToTokens(sequenceOfTokens.ids);
        }
        else if (sequenceOfTokens.bytes.size() > 0)
        {
            sequenceOfTokens.tokens = this->bytesToTokens(sequenceOfTokens.bytes);
        }
    }
    if (sequenceOfTokens.ids.size() == 0)
    {
        sequenceOfTokens.ids = this->tokensToIds(sequenceOfTokens.tokens);
    }
    if (completeBytes && isTrained && sequenceOfTokens.bytes.size() == 0)
    {
        sequenceOfTokens.bytes   = this->idsToBytes(sequenceOfTokens.ids);
        const size_t sizeOfBytes = sequenceOfTokens.bytes.size();
    }
}

//------------------------------------------------------------------------
void TokenIdConverterBase::createVocabulary(
    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> &tempBaseVocab)
{
    if (std::holds_alternative<std::vector<std::vector<std::string>>>(tempBaseVocab))
    {
        const auto &vocabVec = std::get<std::vector<std::vector<std::string>>>(tempBaseVocab);
        vocabBase            = std::vector<std::unordered_map<std::string, int>>(vocabVec.size());
        vocabBaseInv         = std::vector<std::unordered_map<int, std::string>>(vocabVec.size());

        for (int vid = 0; vid < vocabVec.size(); ++vid)
        {
            auto vecCopy = vocabVec[vid];
            vecCopy.insert(vecCopy.begin(), config->specialTokens.begin(), config->specialTokens.end());
            for (const std::string &tok : vecCopy)
            {
                addToVocab(tok, /*specialToken=*/false, vid);
            }
        }
    }
    else
    {
        const auto &vocab                          = std::get<std::vector<std::string>>(tempBaseVocab);
        std::vector<std::string> specialTokensCopy = config->specialTokens;
        for (const auto &tok : specialTokensCopy)
        {
            addToVocab(tok, /*specialToken=*/true);
        }
        for (const auto &tok : vocab)
            addToVocab(tok);
    }
}

//------------------------------------------------------------------------
void TokenIdConverterBase::addToVocab(const std::string &tokenAsString, bool isSpecialToken, int vocabIdx,
                                      const std::u32string &byte)
{
    std::string token = tokenAsString;
    if (isSpecialToken)
    {
        // Split tokenAsString on '_'
        std::vector<std::string> parts = libmusictokUtils::split(token, "_");

        // if there is only one part, then add "None"
        if (parts.size() == 1)
        {
            parts.push_back("None");
        }
        // if there are more than two parts then join all except the last using '-'
        else if (parts.size() > 2)
        {
            // join all parts except the last one
            std::vector<std::string> firstParts(parts.begin(), parts.end() - 1);
            std::string joined = libmusictokUtils::join(firstParts, "-");
            std::string last   = parts.back();
            parts.clear();
            parts.push_back(joined);
            parts.push_back(last);
        }
        // reassemble token using '_' as the join token
        token = libmusictokUtils::join(parts, "_");

        // If not already present in the special tokens, add it.
        if (std::find(config->specialTokens.begin(), config->specialTokens.end(), token) == config->specialTokens.end())
        {
            config->specialTokens.push_back(token);
        }
    }

    // catch errors in inputs
    if (vocabIdx != -1 && !isMultiVoc() && verbose)
    {
        std::clog << "Warning: vocabIdx is passed a value while the tokenIdConverter does not support embedding "
                     "pooling (isMultiVoc is false).\n";
    }
    else if (isMultiVoc() && vocabIdx == -1 && verbose)
    {
        std::clog << "Warning: vocabIdx expected a value because the tokenIdConverter supports embedding pooling "
                     "(isMultiVoc is true).\n";
    }

    auto &dict_vocab =
        (vocabIdx != -1 ? std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase)[vocabIdx]
                        : std::get<std::unordered_map<std::string, int>>(vocabBase));

    // Check if token_str is already in the vocabulary; if so, warn.
    if (dict_vocab.find(token) != dict_vocab.end())
    {
        int token_id = dict_vocab[token];
        if (verbose)
        {
            std::cerr << "Warning: Token " << token << " is already in the vocabulary at idx " << token_id << ".\n";
        }
    }
    // else we want to add the token to the vocabulary
    else if (vocabIdx != -1)
    {
        // As the dicts are unordered, different to the python implementation, we don't necessarily
        // care where the values added are inserted

        // currentSize serves as the next available integer that can be used as an id.
        int idBaseNew =
            static_cast<int>(std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase)[vocabIdx].size());
        (std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase)[vocabIdx]).insert({token, idBaseNew});

        // Not sure why python MIDItok finds the size of the inverse, I'd assumed it'd always be the same...?
        int idBaseInvNew = static_cast<int>(
            std::get<std::vector<std::unordered_map<int, std::string>>>(vocabBaseInv)[vocabIdx].size());
        (std::get<std::vector<std::unordered_map<int, std::string>>>(vocabBaseInv)[vocabIdx])
            .insert({idBaseInvNew, token});
    }
    else
    {
        int idBaseNew = isTrained ? static_cast<int>(model->GetVocabSize())
                                  : static_cast<int>(std::get<std::unordered_map<std::string, int>>(vocabBase).size());
        (std::get<std::unordered_map<std::string, int>>(vocabBase)).insert({token, idBaseNew});

        int idBaseInvNew = static_cast<int>((std::get<std::unordered_map<int, std::string>>(vocabBaseInv)).size());
        (std::get<std::unordered_map<int, std::string>>(vocabBaseInv)).insert({idBaseInvNew, token});

        // Determine the byte for the token.
        std::u32string byte_str;
        if (byte.empty())
        {
            // Convert id_ + CHR_ID_START into a one-character string.
            byte_str = std::u32string(1, static_cast<char16_t>(idBaseNew + CHR_ID_START));
        }
        else
        {
            byte_str = byte;
        }
        // Map the id to its byte and vice versa.
        vocabBaseIdToByte.insert({idBaseNew, byte_str});
        vocabBaseByteToToken.insert({byte_str, token});

        // -----------------------------
        // DOES NOT ADD TO MODEL VOCAB |
        // -----------------------------
        // Add the token to the model's vocabulary. However, we do not have access to it just yet via the tokenizers library...
        //
        // Note: libmusictok should not be used for training tokenizers. When importing a tokenizer via loadJson, all tokens
        // should already be inside the tokenizer's vocabulary.
    }
}

//------------------------------------------------------------------------
int TokenIdConverterBase::getItem(const std::string &token) const
{
    if (isMultiVoc())
    {
        // Get the vector of vocabularies.
        const auto &vocabs = std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase);

        // Check that the token is contained in every vocabulary.
        for (const auto &vocabulary : vocabs)
        {
            if (!vocabulary.contains(token))
            {
                throw std::invalid_argument(
                    "This tokenizer uses multiple vocabularies / embedding pooling. To index it you must either "
                    "provide a token (string) that is within all vocabularies (e.g. special tokens), or a tuple where "
                    "the first element in the index of the vocabulary and the second the element to index.");
            }
        }
        // Assume that the id should be the same for all sub-vocabs
        return vocabs[0].at(token);
    }

    // get the vocabulary
    const auto &vocabulary = std::get<std::unordered_map<std::string, int>>(vocabBase);
    return vocabulary.at(token);
}

//------------------------------------------------------------------------
int TokenIdConverterBase::getItem(const std::string &token, const int &vocabIdx) const
{
    if (!isMultiVoc())
    {
        std::clog << "This tokenizer doesn't use multiple vocabularies / embedding pooling. Therefore please do not "
                     "provide an argument for vocabIdx. Your argument is being ignored";
        return getItem(token);
    }

    const auto &vocabulary = std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase)[vocabIdx];
    return vocabulary.at(token);
}

//------------------------------------------------------------------------
std::string TokenIdConverterBase::getItem(const int &id_) const
{
    // get vocabulary reference
    const auto &vocabulary = std::get<std::unordered_map<int, std::string>>(vocabBaseInv);
    return vocabulary.at(id_);
}

//------------------------------------------------------------------------
std::string TokenIdConverterBase::getItem(const int &id_, const int &vocabIdx) const
{
    if (!isMultiVoc())
    {
        std::clog << "This tokenizer doesn't use multiple vocabularies / embedding pooling. Therefore please do not "
                     "provide an argument for vocabIdx. Your argument is being ignored";
        return getItem(id_);
    }
    // get vocabulary reference
    const auto &vocabulary = std::get<std::vector<std::unordered_map<int, std::string>>>(vocabBaseInv)[vocabIdx];
    return vocabulary.at(id_);
}

//------------------------------------------------------------------------
bool TokenIdConverterBase::isMultiVoc() const
{
    return std::holds_alternative<std::vector<std::unordered_map<std::string, int>>>(this->vocabBase);
}

//------------------------------------------------------------------------
bool TokenIdConverterBase::getIsVocabLoaded()
{
    return this->isVocabLoaded;
}

//------------------------------------------------------------------------
bool TokenIdConverterBase::getIsTrained()
{
    return this->isTrained;
}

//------------------------------------------------------------------------
size_t TokenIdConverterBase::getVocabSize()
{
    if (isMultiVoc())
    {
        assert((std::holds_alternative<std::vector<std::unordered_map<std::string, int>>>(vocabBase)));
        size_t totalSize = 0;
        for (const std::unordered_map<std::string, int> &subVocab :
             std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase))
        {
            totalSize += subVocab.size();
        }
        return totalSize;
    }

    if (isTrained)
    {
        return vocabModel.size();
    }

    return std::get<std::unordered_map<std::string, int>>(vocabBase).size();
}

//------------------------------------------------------------------------
size_t TokenIdConverterBase::getBaseVocabSize()
{
    size_t vocabSize;

    if (std::holds_alternative<std::vector<std::unordered_map<std::string, int>>>(vocabBase))
    {
        return (std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase).size());
    }

    return std::get<std::unordered_map<std::string, int>>(vocabBase).size();
}

//------------------------------------------------------------------------
std::vector<int> TokenIdConverterBase::getTokenIdsOfType(const std::string &tokenType, int vocabId)
{
    if (vocabId > 0)
    {
        if (std::holds_alternative<std::unordered_map<std::string, std::vector<int>>>(tokenTypesIndexes))
        {
            std::clog << "Tokenizer vocabulary does not have multiple "
                         "vocabularies. Your vocabID argument to getTokenIdsOfType is being ignored"
                      << std::endl;
        }
        return std::get<std::unordered_map<std::string, std::vector<int>>>(tokenTypesIndexes).at(tokenType);
    }
    else
    {
        if (std::holds_alternative<std::unordered_map<std::string, std::vector<int>>>(tokenTypesIndexes))
        {
            throw std::invalid_argument("This tokenizer is multivoc. You must specify a vocabId to getTokenIdsOfType");
        }
        return std::get<std::vector<std::unordered_map<std::string, std::vector<int>>>>(tokenTypesIndexes)
            .at(vocabId)
            .at(tokenType);
    }
}

//------------------------------------------------------------------------
std::unordered_map<std::string, std::vector<int>> createForVoc(const std::unordered_map<std::string, int> &voc)
{
    std::unordered_map<std::string, std::vector<int>> types_;
    for (const auto &kv : voc)
    {
        const std::string &event = kv.first;
        int token                = kv.second;

        std::string tokenType = libmusictokUtils::split(event, "_")[0];
        types_[tokenType].push_back(token);
    }
    return types_;
}

//------------------------------------------------------------------------
void TokenIdConverterBase::updateTokenTypesIndexes()
{
    if (isMultiVoc())
    {
        std::vector<std::unordered_map<std::string, std::vector<int>>> result;
        for (const auto &voc_i : std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase))
        {
            result.push_back(createForVoc(voc_i));
        }
        tokenTypesIndexes = std::move(result);
    }
    else
    {
        tokenTypesIndexes = createForVoc(std::get<std::unordered_map<std::string, int>>(vocabBase));
    }
}

//------------------------------------------------------------------------
vocabType TokenIdConverterBase::getVocab() const
{
    return vocabBase;
}

// ----------------
// PRIVATE METHODS |
//------------------------------------------------------------------------
void TokenIdConverterBase::resetState()
{
    // Reset model (resetting unique_ptr deletes the held object)
    model.reset();

    // Configs
    config = nullptr;

    // Vocabulary
    vocabBase    = std::unordered_map<std::string, int>{};
    vocabBaseInv = std::unordered_map<int, std::string>{};
    vocabBaseByteToToken.clear();
    vocabLearnedBytesToTokens.clear();
    vocabBaseIdToByte.clear();
    vocabModel.clear();

    isTrained     = false;
    isVocabLoaded = false;

    continuingSubwordPrefix.clear();
    endOfWordSuffix.clear();
    preTokenizerType.clear();
    preTokenizerReplacement.clear();
}

//------------------------------------------------------------------------
std::vector<int> TokenIdConverterBase::tokensToIds(const std::vector<std::string> &tokens) const
{
    std::vector<int> ids;

    if (tokens.size() == 0)
    {
        return ids;
    }

    // get vocabulary
    const auto &vocabulary = std::get<std::unordered_map<std::string, int>>(vocabBase);
    for (const std::string &token : tokens)
    {
        ids.push_back(vocabulary.at(token));
    }

    return ids;
}

//------------------------------------------------------------------------
std::vector<std::vector<int>> TokenIdConverterBase::tokensToIds(
    const std::vector<std::vector<std::string>> &tokens) const
{
    // make sure tokenizer supports multiple dictionaries
    if (!isMultiVoc())
    {
        throw std::invalid_argument(
            "This tokenizer doesn't support multiple vocabularies / embedding pooling. Please pass tokens of type "
            "std::vector<std::string>, not std::vector<std::vector<std::string>>");
    }

    std::vector<std::vector<int>> ids;

    const auto &vocabularies = std::get<std::vector<std::unordered_map<std::string, int>>>(vocabBase);

    // process each subsequence in turn
    for (const std::vector<std::string> &seq : tokens)
    {
        std::vector<int> idSubSeq;

        int vocabIndexIterator = 0;
        for (const std::string &token : seq)
        {
            idSubSeq.push_back(vocabularies.at(vocabIndexIterator).at(token));
            vocabIndexIterator += 1;
        }

        ids.push_back(idSubSeq);
    }

    return ids;
}

//------------------------------------------------------------------------
TokSeqAPI::Ids TokenIdConverterBase::tokensToIds(const TokSeqAPI::Tokens &tokens) const
{
    if (isMultiVoc())
    {
        return TokSeqAPI::Ids(tokensToIds(tokens.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Ids(tokensToIds(tokens.get()));
    }
}

//------------------------------------------------------------------------
std::vector<std::string> TokenIdConverterBase::eventsToTokens(const std::vector<Event> &events) const
{
    std::vector<std::string> tokens;

    if (events.size() == 0)
    {
        return tokens;
    }

    for (const Event &event : events)
    {
        tokens.push_back(event.str());
    }
    return tokens;
}

//------------------------------------------------------------------------
std::vector<std::vector<std::string>> TokenIdConverterBase::eventsToTokens(
    const std::vector<std::vector<Event>> &events) const
{
    std::vector<std::vector<std::string>> tokens;

    if (events.empty())
    {
        return tokens;
    }
    // For each sub-vector (representing events from a given vocabulary)
    for (const auto &multiEvent : events)
    {
        std::vector<std::string> tokenList;
        for (const Event &event : multiEvent)
        {
            tokenList.push_back(event.str());
        }
        tokens.push_back(tokenList);
    }
    return tokens;
}

//------------------------------------------------------------------------
TokSeqAPI::Tokens TokenIdConverterBase::eventsToTokens(const TokSeqAPI::Events &events) const
{
    if (events.isMultiVoc())
    {
        return TokSeqAPI::Tokens(eventsToTokens(events.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Tokens(eventsToTokens(events.get()));
    }
}

//------------------------------------------------------------------------
std::vector<std::string> TokenIdConverterBase::idsToTokens(const std::vector<int> &ids) const
{
    std::vector<std::string> tokens;

    if (ids.size() == 0)
    {
        return tokens;
    }

    for (int id_ : ids)
    {
        std::string token = getItem(id_);
        tokens.push_back(token);
    }

    return tokens;
}

//------------------------------------------------------------------------
std::vector<std::vector<std::string>> TokenIdConverterBase::idsToTokens(const std::vector<std::vector<int>> &ids) const
{
    std::vector<std::vector<std::string>> tokens;

    if (ids.empty())
    {
        return tokens;
    }

    // For each sub-vector (representing events from a given vocabulary)
    for (const auto &idVec : ids)
    {
        std::vector<std::string> tokenList;
        for (int id_ : idVec)
        {
            std::string token = getItem(id_);
            tokenList.push_back(token);
        }
        tokens.push_back(tokenList);
    }
    return tokens;
}

//------------------------------------------------------------------------
TokSeqAPI::Tokens TokenIdConverterBase::idsToTokens(const TokSeqAPI::Ids &ids) const
{
    if (ids.isMultiVoc())
    {
        return TokSeqAPI::Tokens(idsToTokens(ids.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Tokens(idsToTokens(ids.get()));
    }
}

//------------------------------------------------------------------------
std::vector<Event> TokenIdConverterBase::idsToEvents(const std::vector<int> &ids) const
{
    std::vector<Event> events;

    if (ids.size() == 0)
    {
        return events;
    }

    for (int id_ : ids)
    {
        std::string token                          = getItem(id_);
        const std::vector<std::string> eventParams = libmusictokUtils::split(token, "_");
        Event event                                = Event(eventParams[0], eventParams[1]);
        events.push_back(event);
    }

    return events;
}

//------------------------------------------------------------------------
std::vector<std::vector<Event>> TokenIdConverterBase::idsToEvents(const std::vector<std::vector<int>> &ids) const
{
    std::vector<std::vector<Event>> events;

    if (ids.empty())
    {
        return events;
    }

    // For each sub-vector (representing events from a given vocabulary)
    for (const auto &idVec : ids)
    {
        std::vector<Event> eventList;
        for (int id_ : idVec)
        {
            std::string token                          = getItem(id_);
            const std::vector<std::string> eventParams = libmusictokUtils::split(token, "_");
            Event event                                = Event(eventParams[0], eventParams[1]);
            eventList.push_back(event);
        }
        events.push_back(eventList);
    }
    return events;
}

//------------------------------------------------------------------------
TokSeqAPI::Events TokenIdConverterBase::idsToEvents(const TokSeqAPI::Ids &ids) const
{
    if (ids.isMultiVoc())
    {
        return TokSeqAPI::Events(idsToEvents(ids.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Events(idsToEvents(ids.get()));
    }
}

//------------------------------------------------------------------------
std::vector<std::string> TokenIdConverterBase::bytesToTokens(const std::u32string &bytes) const
{
    std::vector<std::string> tokens;
    if (bytes.empty())
    {
        return tokens;
    }

    // Process each byte in the input string
    for (const char16_t &c : bytes)
    {
        // Convert the char to a string key for lookup
        std::u32string byte_(1, c);

        // Find the corresponding token in the mapped vocabulary
        auto it = vocabLearnedBytesToTokens.find(byte_);
        if (it != vocabLearnedBytesToTokens.end())
        {
            const std::vector<std::string> &tokenVec = it->second;
            for (const std::string &token : tokenVec)
            {
                tokens.push_back(token);
            }
            throw std::runtime_error("LibMusicTok: bytesToTokens not implemented correctly yet.");
        }
        else
        {
            throw std::runtime_error("LibMusicTok: byte " + libmusictokUtils::utf32_to_utf8(byte_) +
                                     " not found in learned vocab dictionary.");
        }
    }
    return tokens;
}

//------------------------------------------------------------------------
std::vector<std::vector<std::string>> TokenIdConverterBase::bytesToTokens(
    const std::vector<std::u32string> &bytes) const
{
    std::vector<std::vector<std::string>> tokensVec;
    if (bytes.empty())
    {
        return tokensVec;
    }

    for (const std::u32string &bytesList : bytes)
    {
        tokensVec.push_back(bytesToTokens(bytesList));
    }
    return tokensVec;
}

//------------------------------------------------------------------------
TokSeqAPI::Tokens TokenIdConverterBase::bytesToTokens(const TokSeqAPI::Bytes &bytes) const
{
    if (bytes.isMultiVoc())
    {
        return TokSeqAPI::Tokens(bytesToTokens(bytes.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Tokens(bytesToTokens(bytes.get()));
    }
}

//------------------------------------------------------------------------
std::vector<Event> TokenIdConverterBase::bytesToEvents(const std::u32string &bytes) const
{
    std::vector<Event> events;
    if (bytes.empty())
    {
        return events;
    }

    // Process each byte in the input string
    for (const char16_t &c : bytes)
    {
        // Convert the char to a string key for lookup
        std::u32string byte_(1, c);

        // Find the corresponding token in the mapped vocabulary
        auto it = vocabLearnedBytesToTokens.find(byte_);
        if (it != vocabLearnedBytesToTokens.end())
        {
            const std::vector<std::string> &tokenVec = it->second;
            for (std::string token : tokenVec)
            {
                std::vector<std::string> eventParams = libmusictokUtils::split(token, "_");
                Event event                          = Event(eventParams[0], eventParams[1]);
                events.push_back(event);
            }
            throw std::runtime_error("LibMusicTok: bytesToTokens not implemented correctly yet.");
        }
        else
        {
            throw std::runtime_error("LibMusicTok: byte " + libmusictokUtils::utf32_to_utf8(byte_) +
                                     " not found in learned vocab dictionary.");
        }
    }
    return events;
}

//------------------------------------------------------------------------
std::vector<std::vector<Event>> TokenIdConverterBase::bytesToEvents(const std::vector<std::u32string> &bytes) const
{
    std::vector<std::vector<Event>> eventsVec;
    if (bytes.empty())
    {
        return eventsVec;
    }

    for (const std::u32string &bytesList : bytes)
    {
        eventsVec.push_back(bytesToEvents(bytesList));
    }
    return eventsVec;
}

//------------------------------------------------------------------------
TokSeqAPI::Events TokenIdConverterBase::bytesToEvents(const TokSeqAPI::Bytes &bytes) const
{
    if (bytes.isMultiVoc())
    {
        return TokSeqAPI::Events(bytesToEvents(bytes.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Events(bytesToEvents(bytes.get()));
    }
}

//------------------------------------------------------------------------
std::u32string TokenIdConverterBase::idsToBytes(const std::vector<int> &ids) const
{
    std::u32string bytes;
    if (ids.empty())
    {
        return bytes;
    }

    for (int id_ : ids)
    {
        bytes = bytes + vocabBaseIdToByte.at(id_);
    }

    return bytes;
}

//------------------------------------------------------------------------
std::vector<std::u32string> TokenIdConverterBase::idsToBytes(const std::vector<std::vector<int>> &ids) const
{
    std::vector<std::u32string> bytesVec;
    if (ids.empty())
    {
        return bytesVec;
    }

    for (const std::vector<int> &idList : ids)
    {
        bytesVec.push_back(idsToBytes(idList));
    }

    return bytesVec;
}

//------------------------------------------------------------------------
TokSeqAPI::Bytes TokenIdConverterBase::idsToBytes(const TokSeqAPI::Ids &ids) const
{
    if (ids.isMultiVoc())
    {
        return TokSeqAPI::Bytes(idsToBytes(ids.getMultiVoc()));
    }
    else
    {
        return TokSeqAPI::Bytes(idsToBytes(ids.get()));
    }
}

//------------------------------------------------------------------------
std::vector<std::u32string> TokenIdConverterBase::idsToBytesVec(const std::vector<int> &ids) const
{
    std::vector<std::u32string> bytesVec;
    if (ids.empty())
    {
        return bytesVec;
    }

    for (int id_ : ids)
    {
        bytesVec.push_back(vocabBaseIdToByte.at(id_));
    }

    return bytesVec;
}

//------------------------------------------------------------------------
std::vector<std::vector<std::u32string>> TokenIdConverterBase::idsToBytesVec(
    const std::vector<std::vector<int>> &ids) const
{
    std::vector<std::vector<std::u32string>> bytesVec;
    if (ids.empty())
    {
        return bytesVec;
    }

    for (const std::vector<int> &idList : ids)
    {
        bytesVec.push_back(idsToBytesVec(idList));
    }

    return bytesVec;
}

//------------------------------------------------------------------------
void TokenIdConverterBase::createVocabLearnedBytesToTokens()
{
    // Loop through every entry in vocabModel
    for (const auto &pair : vocabModel)
    {
        const std::u32string &k = pair.first;
        std::u32string key      = k;

        // Remove continuingSubwordPrefix if it exists.
        if (!continuingSubwordPrefix.empty() && key.size() >= continuingSubwordPrefix.size() &&
            key.compare(0, continuingSubwordPrefix.size(), continuingSubwordPrefix) == 0)
        {
            key.erase(0, continuingSubwordPrefix.size());
        }

        // Remove endOfWordSuffix if it exists.
        if (!endOfWordSuffix.empty() && key.size() >= endOfWordSuffix.size() &&
            key.compare(key.size() - endOfWordSuffix.size(), endOfWordSuffix.size(), endOfWordSuffix) == 0)
        {
            key.erase(key.size() - endOfWordSuffix.size(), endOfWordSuffix.size());
        }

        // If the pre-tokenizer is of type Metaspace, then remove the replacement prefix.
        if (preTokenizerType.compare("Metaspace") == 0)
        {
            if (!preTokenizerReplacement.empty() && key.size() >= preTokenizerReplacement.size() &&
                key.compare(0, preTokenizerReplacement.size(), preTokenizerReplacement) == 0)
            {
                key.erase(0, preTokenizerReplacement.size());
            }
        }

        // Convert each byte in key into its corresponding token string using vocabBaseByteToToken.
        std::vector<std::string> tokenVector;

        // For each wide-character in key, convert it to a one-character std::string.
        for (const char16_t &wc : key)
        {
            // Convert char to a one-character std::string.
            std::u32string byteStr(1, wc);

            // Look up the token in the vocabBaseByteToToken mapping.
            tokenVector.push_back(vocabBaseByteToToken.at(byteStr));
        }
        // Map the original vocab key to the concatenated token string.
        vocabLearnedBytesToTokens[k] = tokenVector;
    }
}

//------------------------------------------------------------------------
// 							 TokenIdConverter
//------------------------------------------------------------------------

//------------------------------------------------------------------------
std::vector<TokSequence> TokenIdConverter::splitTokSeqPerTrack(const TokSequence &tokSeq, bool keepTrackTokens) const
{
    throw std::runtime_error("TokenIdConverter::splitTokSeqPerTrack should not be called.");
    // This should never be reached.
    return {};
}

//------------------------------------------------------------------------
// 							MMM TokenIdConverter
//------------------------------------------------------------------------

//------------------------------------------------------------------------
void MMMTokenIdConverter::encodeTokenIds(TokSequence &sequenceOfTokens) const
{
    if (config->encodeIdsSplit == "no")
    {
        TokenIdConverterBase::encodeTokenIds(sequenceOfTokens);
    }
    else
    {
        std::vector<TokSequence> seqsSplit = splitTokSeqPerTrack(sequenceOfTokens, true);
        TokenIdConverterBase::encodeTokenIds(seqsSplit);
        sequenceOfTokens.ids.clear();

        for (const TokSequence &subSequence : seqsSplit)
        {
            sequenceOfTokens.ids.insert(sequenceOfTokens.ids.end(), subSequence.ids.begin(), subSequence.ids.end());
        }
        sequenceOfTokens.areIdsEncoded = true;
    }
}

//------------------------------------------------------------------------
std::vector<TokSequence> MMMTokenIdConverter::splitTokSeqPerTrack(const TokSequence &tokSeq, bool keepTrackTokens) const
{
    const std::vector<int> &ids = tokSeq.ids.get();
    std::vector<size_t> trackTokensIdx;
    assert((std::holds_alternative<std::unordered_map<std::string, int>>(vocabBase)));

    int trackStartId = std::get<std::unordered_map<std::string, int>>(vocabBase).at("Track_Start");

    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (ids[i] == trackStartId)
        {
            trackTokensIdx.push_back(i);
        }
    }

    if (trackTokensIdx.empty())
    {
        return {tokSeq};
    }

    std::vector<TokSequence> tokseqs;
    for (size_t i = 0; i < trackTokensIdx.size(); ++i)
    {
        size_t idxStart = keepTrackTokens ? trackTokensIdx[i] : trackTokensIdx[i] + 1;
        std::optional<size_t> idxEnd;

        if (i + 1 == trackTokensIdx.size())
        {
            idxEnd = std::nullopt; // until end
        }
        else if (keepTrackTokens)
        {
            idxEnd = trackTokensIdx[i + 1];
        }
        else
        {
            idxEnd = trackTokensIdx[i + 1] - 1;
        }

        TokSequence slice;

        if (!idxEnd.has_value())
        {
            slice = tokSeq.sliceUntilEnd(idxStart);
        }
        else
        {
            slice = tokSeq.slice(idxStart, idxEnd.value());
        }
        tokseqs.push_back(slice);
    }
    return tokseqs;
}

} // namespace libmusictok
