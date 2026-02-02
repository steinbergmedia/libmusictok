//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tokenizers
// Filename    : include/libmusictok/tokenizations/music_tokenizer.h
// Created by  : Steinberg, 02/2025
// Description : Music tokenizer base class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/attribute_control.h"
#include "libmusictok/common.h"
#include "libmusictok/constants.h"
#include "libmusictok/tok_sequence.h"
#include "libmusictok/token_id_converter.h"
#include "libmusictok/tokenizer_config.h"
#include "libmusictok/utility_functions.h"

#include "symusic.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

//------------------------------------------------------------------------
namespace libmusictok {

using StringIntMap    = std::unordered_map<std::string, int>;
using StringIntMapVec = std::vector<StringIntMap>;
using TokSequenceVec  = std::vector<TokSequence>;
using AttributeControlsIndexesType =
    std::unordered_map<int, std::unordered_map<int, std::variant<std::vector<int>, bool>>>;

//------------------------------------------------------------------------
enum class TokenizerType
{
    kBase = 0,
    kREMI,
    kTSD,
    kMIDILike,
    kMMM,
    kStructured,
    kOctuple,
    kMuMIDI,
    kCPWord,
    kPerTok
};

//------------------------------------------------------------------------
/** MusicTokenizer - Base Class

 This tokenizer is not usable, it's just a base class upon which complete tokenizers, such as REMI, are built.
 Note that this class is a C++ implementation of the class of the same name in MIDITok, with the same API.
 */
//------------------------------------------------------------------------
class MusicTokenizer
{
public:
    //------------------------------------------------------------------------
    MusicTokenizer(TokenizerType type, bool verbose);

    virtual ~MusicTokenizer() = default;

    /** Adds an Attribute Control token to the vocabulary and sets up the score processing
	to compute those tokens while tokenizing music. */
    void addAttributeControl(std::shared_ptr<AttributeControl> attributeControl);

    ///< Returns the id of the Pad token ("PAD_None")
    int getPadToken() const;

    /** Returns true if the tokenizer uses individual tokens for NoteOn and NoteOff messages.
	Returns false if the tokenizer only encodes the onsets. */
    bool getNoteOnOff() const;

    ///< Returns the vocabulary of the tokenizer
    std::variant<StringIntMapVec, StringIntMap> getVocab() const;

    ///< Returns the size of the Vocabulary
    size_t getVocabSize() const;

    ///< Returns the special tokens
    std::vector<std::string> getSpecialTokens() const;

    ///< Returns the special tokens' ids
    std::vector<int> getSpecialTokenIds() const;

    ///< Checks if the tokenizer is trained (e.g. with BPE)
    bool isTrained() const;

    /**
	 Pre-process a ``Score`` object to resample its time and events values.

	 This method is called before parsing a ``Score``'s contents for tokenization.
	 Its notes attributes (times, pitches, velocities) will be downsampled and
	 sorted, duplicated notes removed, as well as tempos. Empty tracks (with no
	 note) will be removed from the ``Score`` object. Notes with pitches
	 outside ``config.pitchRange`` will be deleted. Tracks with programs not
	 supported by the tokenizer will be deleted.

	 This method is **not inplace** and does not alter the provided ``Score`` object.

	 @param score Score object to preprocess.
	 @return the preprocessed Score
	 */
    ScoreType preprocessScore(const ScoreType &score) const;

    ///< Loads new settings for the current tokenizer from file. It is not recommended to use this method.
    virtual void loadNewConfigFile(const std::filesystem::path &tokenizerConfigFile);

    /**
	 Tokenize a music file (MIDI/abc), given as a ``Score`` or a file path.

	 You can provide a ``Path`` to the file to tokenize, or a ``Score``
	 object.
	 This method returns a (list of) :class:`miditok.TokSequence`.

	 If you are implementing your own tokenization by subclassing this class,
	 **override the protected** ``_score_to_tokens`` **method**.

	 @param score the ``Score`` object to convert.
	 @param encodeIds the backbone model (BPE, Unigram, WordPiece) will encode the
		 tokens and compress the sequence. Can only be used if the tokenizer has been
		 trained. (default: ``True``)
	 @param no_preprocess_score whether to preprocess the ``symusic.Score``. If this
		 argument is provided as ``True``, make sure that the corresponding music
		 file / ``symusic.Score`` has already been preprocessed by the tokenizer
		 (:py:func:`miditok.MusicTokenizer.preprocess_score`) or that its content is
		 aligned with the tokenizer's vocabulary, otherwise the tokenization is
		 likely to crash. This argument is useful in cases where you need to use the
		 preprocessed ``symusic.Score`` along with the tokens to not have to
		 preprocess it twice as this method preprocesses it inplace.
		 (default: ``False``)
	 @param attribute_controls_indexes indices of the attribute controls to compute
		 and associated tracks and bars. This argument has to be provided as a
		 dictionary mapping track indices to dictionaries mapping attribute control
		 indices (indexing ``tokenizer.attributeControls``) to a sequence of bar
		 indexes if the AC is "bar-level" or anything if it is "track-level".
		 Its structure is as:
		 ``{track_idx: {ac_idx: Any (track ac) | [bar_idx, ...] (bar ac)}}``
		 This argument is meant to be used when training a model in order to make it
		 learn to generate tokens accordingly to the attribute controls. For maximum
		 safety, it should be used with ``noPreprocessScore`` with an already
		 preprocessed ``symusic.Score`` in order to make sure that the provided
		 tracks indexes will remain correct as the preprocessing might delete or
		 merge tracks depending on the tokenizer's configuration.
	 @return a `miditok.TokSequence` if ``tokenizer.one_token_stream`` is
		 ``True``, else a vector of `miditok.TokSequence` objects.
	 */
    virtual std::variant<TokSequence, std::vector<TokSequence>> encode(
        const ScoreType &score, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const;

    // As above, passing in a score Path instead
    virtual std::variant<TokSequence, std::vector<TokSequence>> encode(
        const std::filesystem::path &scorePath, bool encodeIds = true, bool noPreprocessScore = false,
        AttributeControlsIndexesType attributeControlsIndexes = {}) const;

    ///< Fills in the missing fields in the TokSequence object
    void complete_sequence(TokSequence &seq, bool complete_bytes = false) const;

    /**
	Detokenize one or several sequences of tokens into a ``Score``.

	You can give the tokens sequences either as ``TokSequence``
	objects or as vectors of integers (see the overloads).
	The Score's time division will be the same as the tokenizer's:
	``tokenizer.timeDivision``.

	@param tokens tokens to convert. Can be either a vector of
		`LibMusicTok.TokSequence` or a vector of ints. The first dimension
		represents tracks, unless the tokenizer handle tracks altogether as a
		single token sequence (``tokenizer.oneTokenStream == True``).
	@param programs programs of the tracks. If none is given, will default to
		piano, program 0. (default: ``{}``)
	@param outputPath path to save the file. (default: "")
	@return: the ``Score`` object.
	 */
    ScoreType decode(TokSequence &tokens, const std::vector<std::tuple<int, bool>> &programs = {},
                     const std::filesystem::path &outputPath = "") const;

    ScoreType decode(TokSequenceVec &tokens, const std::vector<std::tuple<int, bool>> &programs = {},
                     const std::filesystem::path &outputPath = "") const;

    ScoreType decode(const std::vector<int> &tokens, const std::vector<std::tuple<int, bool>> &programs = {},
                     const std::filesystem::path &outputPath = "") const;

    ScoreType decode(const std::vector<std::vector<int>> &tokens,
                     const std::vector<std::tuple<int, bool>> &programs = {},
                     const std::filesystem::path &outputPath            = "") const;

    ///< Returns the IDs of a given type. vocabId = -1 for non-MultiVoc
    std::vector<int> getTokenIdsOfType(const std::string &tokenType, int vocabId = -1) const;

    /** Adds a token to the vocabulary. Keep vocabIdx = -1 for single-vocabulary tokenizers.
	You may specify if the token is special, and if it should have a specific byte. */
    void addToVocab(const Event &token, bool specialToken = false, int vocabIdx = -1,
                    std::u32string byte = std::u32string());

    virtual void addToVocab(const std::string &token, bool specialToken = false, int vocabIdx = -1,
                            std::u32string byte = std::u32string());

    bool scoreHasTimeSignaturesNotInVocab(const ScoreType &score) const;

    ///< Encode a TokSequence with BPE, Unigram or WordPiece.
    void encodeTokenIds(TokSequence &sequence) const;

    ///< Encode TokSequences with BPE, Unigram or WordPiece.
    void encodeTokenIds(TokSequenceVec &sequence) const;

    ///< Decode a TokSequence with BPE, Unigram or WordPiece.
    void decodeTokenIds(TokSequence &sequence) const;

    ///< Decode TokSequences with BPE, Unigram or WordPiece.
    void decodeTokenIds(TokSequenceVec &sequence) const;

    /**
	 Return the ratio of errors of prediction in a sequence of tokens.

	 Check if a sequence of tokens is made of good token types successions and
	 returns the error ratio (lower is better).

	 @param tokens sequence of tokens to check.
	 @return the error ratio (lower is better).
	 */
    float tokensErrors(const std::vector<int> &tokens) const;

    float tokensErrors(TokSequence tokens) const;

    std::vector<float> tokensErrors(const std::vector<std::vector<int>> &tokens) const;

    std::vector<float> tokensErrors(const TokSequenceVec &tokens) const;

    ///< Checks if the tokenizer has multiple vocabularies
    bool isMultiVoc() const;

    ///< Returns the length of the vocabulary
    std::variant<int, std::vector<int>> getVocabLength() const;

    /**
	 Return the i/o format of the tokenizer.

	 The characters for each dimension returned are:
	 * ``I``: track or instrument;
	 * ``T``: token, or time step;
	 * ``C``: class of token, when using embedding pooling.

	 @return i/o format of the tokenizer, as a tuple of strings which represent
	 */
    std::string ioFormat() const;

    ///< Set the verbosity of the tokenizer (false to disable warnings)
    void setVerbose(bool verbose);

    ///< Returns the type of the tokenizer. Mainly for internal use in testing
    TokenizerType getType() const;

    ///< Returns the default tempo of the tokenizer
    double getDefaultTempo() const;

    // Configs
    TokenizerConfig config;

protected:
    virtual void loadFromJson(const std::filesystem::path &tokenizerFile);

    void addNoteTokensToVocabList(std::vector<std::string> &vocab) const;

    void addAdditionalTokensToVocabList(std::vector<std::string> &vocab) const;

    static std::pair<int, int> parseTokenTimeSignature(const std::string &);

    bool startsWith(const std::string &str, const std::string &prefix) const;

    std::pair<std::vector<std::tuple<int, int, int>>, std::vector<int>> timeTicksToTokens(int duration,
                                                                                          int ticksPerBeat,
                                                                                          bool rest = false) const;

    virtual std::vector<Event> createTrackEvents(
        const TrackType &track, const std::vector<std::pair<int, int>> &ticksPerBeat, symusic::i32 timeDivision,
        const std::vector<int> &ticksBars, const std::vector<int> &ticksBeats,
        const std::unordered_map<int, std::variant<std::vector<int>, bool>> &attributeControlIndexes = {}) const;

    virtual Event createDurationEvent(const NoteType &note, int program,
                                      const std::vector<std::pair<int, int>> &ticksPerBeat, int tpbIdx) const;

    virtual std::variant<TokSequence, std::vector<TokSequence>> scoreToTokens(
        const ScoreType &score, const AttributeControlsIndexesType &attributeControlIndexes = {}) const;

    std::vector<Event> createGlobalEvents(const ScoreType &score) const;

    void insertProgramChangeEvents(std::vector<Event> &events) const;

    virtual void sortEvents(std::vector<Event> &events) const;

    virtual std::vector<std::tuple<int, int, int>> createDurationTuples();

    virtual ScoreType resampleScore(const ScoreType &score, int newTpq,
                                    symusic::pyvec<TimeSigType> &timeSignaturesCopy) const;

    virtual void adjustDurations(symusic::pyvec<NoteType> &notes,
                                 const std::vector<std::pair<int, int>> &ticksPerBeat) const;

    virtual void adjustDurations(symusic::pyvec<PedalType> &pedals,
                                 const std::vector<std::pair<int, int>> &ticksPerBeat) const;

    int timeTokenToTicks(const std::tuple<int, int, int> &tokenDuration, int ticksPerBeat) const;

    void updateTokenizer();

    virtual void tweakConfigBeforeCreatingVoc() = 0;

    void addSpecialTokensToTypesGraph();

    int minRest(int ticksPerBeat) const;

    virtual std::variant<std::vector<Event>, std::vector<std::vector<Event>>> addTimeEvents(
        const std::vector<Event> &events, symusic::i32 timeDivision) const = 0;

    virtual std::unordered_map<std::string, std::set<std::string>> createTokensTypesGraph() = 0;

    virtual std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> createBaseVocabulary() = 0;

    virtual ScoreType tokensToScore(std::variant<TokSequence, TokSequenceVec> tokens,
                                    const std::vector<std::tuple<int, bool>> &programs = {}) const = 0;

    virtual int tokensErrorsInternal(const TokSeqAPI::Tokens &tokens) const;

    void disableAttributeControls();

    std::vector<std::string> createChordsTokens() const;

    // vars
    std::vector<std::pair<int, int>> timeSignatures;
    int timeDivision;
    std::unordered_map<int, std::unordered_map<std::string, int>> tpbTokensToTicks;
    std::unordered_map<int, std::unordered_map<int, std::string>> tpbTicksToTokens;
    std::unordered_map<int, std::unordered_map<std::string, int>> tpbRestsToTicks;
    std::unordered_map<int, std::vector<symusic::i32>> tpbToTimeArray;
    std::unordered_map<int, std::vector<symusic::i32>> tpbToRestArray;
    bool verbose = true;
    bool oneTokenStream;
    std::unique_ptr<TokenIdConverterBase> tic;
    bool noteOnOff = false;

    std::vector<std::tuple<int, int, int>> durations;
    std::vector<std::tuple<int, int, int>> rests;
    std::vector<double> tempos;
    std::vector<double> temposMspq;
    double defaultTempo;
    int firstBeatRes;
    std::unordered_map<int, int> tpbPerTs;
    std::vector<symusic::i32> pitchBends;
    std::vector<symusic::i8> velocities;
    std::vector<std::shared_ptr<AttributeControl>> attributeControls;
    std::unordered_map<std::string, std::set<std::string>> tokensTypesGraph;

private:
    friend class MMM;

    std::unordered_map<int, int> createTpbPerTs() const;

    std::vector<std::tuple<int, int, int>> createRests() const;

    std::vector<double> createTempos() const;

    std::vector<symusic::i32> createPitchBends() const;

    std::unordered_map<int, std::vector<symusic::i32>> createTpbToTicksArray(bool rest = false) const;

    std::unordered_map<int, std::unordered_map<std::string, int>> createTpbTokensToTicks(bool rest = false) const;

    std::unordered_map<int, std::unordered_map<int, std::string>> createTpbTicksToTokens() const;

    void createVocabulary();

    void initialiseAttributeControls();

    void filterUnsupportedTimeSignatures(symusic::pyvec<TimeSigType> &timeSignatures) const;

    std::vector<std::pair<int, int>> createTimeSignatures() const;

    std::vector<std::pair<int, int>> getScoreResamplingFactor(const ScoreType &score) const;

    void preprocessTimeSignatures(symusic::pyvec<TimeSigType> &timeSignatureVec, symusic::i32 &timeDivision) const;

    void preprocessNotes(TrackType &track, std::vector<std::pair<int, int>> tpqResamplingFactors,
                         const std::vector<std::pair<int, int>> &ticksPerBeat, int minDuration = 1) const;

    symusic::shared<symusic::pyvec<PitchBendType>> preprocessPitchBends(
        const symusic::pyvec<PitchBendType> &bends,
        const std::vector<std::pair<int, int>> &tpqResamplingFactors = {}) const;

    symusic::shared<symusic::pyvec<PedalType>> preprocessPedals(
        const symusic::pyvec<PedalType> &pedals, const std::vector<std::pair<int, int>> &tpqResamplingFactors = {},
        const std::vector<std::pair<int, int>> &ticksPerBeat = {}, int minDuration = 1) const;

    symusic::shared<symusic::pyvec<TempoType>> preprocessTempos(
        symusic::pyvec<TempoType> &tempos_, const std::vector<std::pair<int, int>> &tpqResamplingFactors = {}) const;

    std::vector<std::pair<int, int>> convertResamplingRatiosTicksToIdx(
        const std::vector<std::pair<int, int>> &resamplingFactors, const std::vector<int> &timeArray) const;

    void adjustTimeToTpb(std::vector<int> &timeArray, const std::vector<std::pair<int, int>> &resamplingFactors,
                         int minDuration = -1) const;

    template <typename T>
    void adjustOffsetSpanningAcrossTimeSig(symusic::pyvec<T> &notesOrPedals,
                                           const std::vector<std::pair<int, int>> &tpqResamplingFactors) const;

    TokSequence convertSequenceToTokseq(const std::vector<int> &idsVec) const;

    TokSequenceVec convertSequenceToTokseq(const std::vector<std::vector<int>> &idsVecVec) const;

    TokSequence convertSequenceToTokseq(const std::vector<std::string> &tokenStrVec) const;

    TokSequenceVec convertSequenceToTokseq(const std::vector<std::vector<std::string>> &tokenStrVecVec) const;

    TokSequence convertSequenceToTokseq(const std::vector<Event> &eventVec) const;

    TokSequenceVec convertSequenceToTokseq(const std::vector<std::vector<Event>> &eventVecVec) const;

    bool areIdsEncoded(const std::vector<int> &ids) const;

    void preprocessTokseqBeforeDecoding(TokSequence &tokSeq) const;

    void createControlsForPedals(ScoreType &score) const;

    // vars
    TokenizerType type = TokenizerType::kBase;
};

} // namespace libmusictok
