//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/utility_functions.h
// Created by  : Steinberg, 02/2025
// Description : Provides some utility functions that are common in the
// 		libmusictok implementation.
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/common.h"
#include "libmusictok/constants.h"
#include "libmusictok/tok_sequence.h"
#include "symusic.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

namespace libmusictokUtils {

// --- IO ---

///< Loads a json object from file
nlohmann::json loadJson(const std::filesystem::path &jsonFilePath);

///< Loads an ordered_json object from file
nlohmann::ordered_json loadOrderedJson(const std::filesystem::path &jsonFilePath);

///< Loads a score object from a path to a MIDI file
libmusictok::ScoreType loadScoreFromMidi(const std::filesystem::path &scoreFile);

///< Saves a MIDI file from a score object
void saveMidiFromScore(const libmusictok::ScoreType &score, const std::filesystem::path &outputPath);

// --- Object manipulation ---

///< Splits a string at the given delimiter.
std::vector<std::string> split(const std::string &s, const std::string &delimiter);

///< Splits a string into its duration tuple.
std::tuple<int, int, int> parseDuration(const std::string &durationStr);

///< Joins a vector of strings using the given delimiter.
std::string join(const std::vector<std::string> &parts, const std::string &delimiter);

///< Joins a vector of strings using the given delimiter.
std::string join(const std::tuple<int, int, int> &parts, const std::string &delimiter);

///< Rounds to two decimals and returns a string
std::string roundToTwoDecimals(double value);

///< Convert from quarters per minute (BPM) to milliseconds per quarter
std::vector<double> tempoQpmToMspq(const std::vector<double> &tempoQpm);

///< Merges tracks with the same program id
void mergeSameProgramTracks(symusic::shared<symusic::vec<symusic::shared<libmusictok::TrackType>>> &tracks,
                            bool effects = true);

///< Merges given tracks
symusic::shared<libmusictok::TrackType> mergeTracks(
    const symusic::vec<symusic::shared<libmusictok::TrackType>> &tracksToMerge, bool effects = true);

///< Removes duplicate notes from a vector of notes
void removeDuplicatedNotes(symusic::pyvec<libmusictok::NoteType> &notes, bool considerDuration = false);

/**
 Detect chords in a vector of notes.

 Make sure notes are sorted by time then pitch before.
 This method works by iterating over each note, find if it played with other notes,
 and if it forms a chord from the chord maps. **It does not consider chord
 inversion.**
 @param notes notes to analyse (sorted by starting time, them pitch).
 @param ticksPerBeat array indicating the number of ticks per beat per
	time signature denominator section. The numbers of ticks per beat depend on the
	time signatures of the Score being parsed. The array has a shape ``(N,2)``, for
	``N`` changes of ticks per beat, and the second dimension representing the end
	tick of each section and the number of ticks per beat respectively.
 @param chordMaps list of chord maps, to be given as a dictionary where keys are
	chord qualities (e.g. "maj") and values pitch maps as tuples of integers
	(e.g. {0, 4, 7}). You can use ``libmusictok.constants.CHORD_MAPS`` as an example.
 @param program program of the track of the notes. Used to specify the program when
	 creating the Event object. (default: None)
 @param specifyRootNote the root note of each chord will be specified in
	 Events/tokens. Tokens will look as "Chord_C:maj". (default: True)
 @param beatRes beat resolution, i.e. number of samples per beat (default 4).
 @param onsetOffset maximum offset (in samples) ∈ N separating notes starts to
	 consider them starting at the same time / onset (default is 1).
 @param unknownChordsNumNotesRange range of number of notes to represent unknown
	 chords. If you want to represent chords that does not match any combination in
	 `chordMaps`, use this argument. Leave ``None`` to not represent unknown
	 chords. (default: None)
 @param simulNotesLimit number of simultaneous notes being processed when looking
	 for a chord this parameter allows to speed up the chord detection, and must be
	 >= 5 (default 10).
 @return the detected chords as Event objects.
 */
std::vector<libmusictok::Event> detectChords(symusic::shared<symusic::pyvec<libmusictok::NoteType>> notes,
                                             const std::vector<std::pair<int, int>> &ticksPerBeat,
                                             const std::vector<std::pair<std::string, std::vector<int>>> &chordMaps,
                                             int program = -1, bool specifyRootNote = true, int beatRes = 4,
                                             int onsetOffset = 1, std::vector<int> unknownChordsNumNotesRange = {},
                                             int simulNotesLimit = 10);

///< Reduces the length of the earlier overlapping notes to have them end before the next ones
void fixOffsetsOverlappingNotes(symusic::pyvec<libmusictok::NoteType> &notes);

///< Removes duplicate strings while presserving order of the input array
std::vector<std::string> removeDuplicatesPreserveOrder(const std::vector<std::string> &input);

// --- UTF conversion ---

///< Converts UTF-8 strings to UTF-16 strings (from std::string to std::u16string)
std::u16string utf8_to_utf16(const std::string &str);

///< Converts UTF-16 strings to UTF-8 strings (from std::u16string to std::string)
std::string utf16_to_utf8(const std::u16string &utf16_str);

///< Converts UTF-8 strings to UTF-32 strings (from std::string to std::u32string)
std::u32string utf8_to_utf32(const std::string &str);

///< Converts UTF-32 strings to UTF-8 strings (from std::u32string to std::string)
std::string utf32_to_utf8(const std::u32string &utf32_str);

// --- Checks ---

///< Checks if two time signatures are equal
bool areTimeSignaturesEqual(const libmusictok::TimeSigType &ts1, const libmusictok::TimeSigType &ts2);

///< Checks if the track object is empty
bool isTrackEmpty(const libmusictok::TrackType &track, bool checkControls = false, bool checkPedals = false,
                  bool checkPitchBends = false);

// --- NumPy implementations ---

///< Implementation of get_closest from NumPy
std::vector<symusic::i32> npGetClosest(const std::vector<symusic::i32> &referenceArray,
                                       const std::vector<symusic::i32> &valueArray);

///< Implementation of get_closest from NumPy
std::vector<symusic::i8> npGetClosest(const std::vector<symusic::i8> &referenceArray,
                                      const std::vector<symusic::i8> &valueArray);

///< Implementation of get_closest from NumPy
std::vector<double> npGetClosest(const std::vector<double> &referenceArray, const std::vector<double> &valueArray);

///< Implementation of get_closest from NumPy
symusic::i32 npGetClosest(const std::vector<symusic::i32> &referenceArray, const symusic::i32 &value);

///< Implementation of get_closest from NumPy
symusic::i8 npGetClosest(const std::vector<symusic::i8> &referenceArray, const symusic::i8 &value);

///< Implementation of get_closest from NumPy
double npGetClosest(const std::vector<double> &referenceArray, const double &value);

///< Implementation of NumPy's histogram
template <typename T> std::vector<size_t> histogram(const std::vector<T> &data, const std::vector<T> &bins);

///< Implementation of NumPy's linspace
template <typename T> std::vector<double> linspace(T startIn, T endIn, int numIn);

///< Rounding function that matches Python, rounding .5 to the nearest even integer.
double bankersRound(double value);

// --- Ticks, Bars and Beats ---

///< Adds ticks of bars and beats to a TokSequence object
void addBarBeatsTicksToTokseq(libmusictok::TokSequence &tokseq, libmusictok::ScoreType &score);

///< Adds ticks of bars and beats to a TokSequence object
void addBarBeatsTicksToTokseq(libmusictok::TokSequence &tokseq, std::vector<int> &barTicks,
                              std::vector<int> &beatTicks);

///< Adds ticks of bars and beats to a TokSequence object
void addBarBeatsTicksToTokseq(std::vector<libmusictok::TokSequence> &tokseqs, libmusictok::ScoreType &score);

///< Adds ticks of bars and beats to a TokSequence object
void addBarBeatsTicksToTokseq(std::vector<libmusictok::TokSequence> &tokseqs, std::vector<int> &barTicks,
                              std::vector<int> &beatTicks);

///< Gets the ticks of the beginnings of bars. If onlyNotesOnsets is true, ending bars without any onsets will be ignored
std::vector<int> getBarsTicks(const libmusictok::ScoreType &score, bool onlyNotesOnsets = true);

///< Gets the ticks of the beginnings of beats. If onlyNotesOnsets is true, ending beats without any onsets will be ignored
std::vector<int> getBeatsTicks(const libmusictok::ScoreType &score, bool onlyNotesOnsets = true);

///< Computes the number of ticks per beat based on the time signature denominator and the ticks per quarter note
int computeTicksPerBeat(int timeSignatureDenominator, int timeDiv);

///< Computes the number of ticks per bar given the time signature and the ticks per quarter note
int computeTicksPerBar(const libmusictok::TimeSigType &timeSig, int timeDiv);

///< Computes the number of ticks per bar given the time signature and the ticks per quarter note
int computeTicksPerBar(const std::pair<int, int> &timeSig, int timeDiv);

///< Gets the ticks per beat from a given score object
std::vector<std::pair<int, int>> getScoreTicksPerBeat(const libmusictok::ScoreType &score);

namespace Internal {

void checkInst(int prog, std::map<int, libmusictok::TrackType> &tracks);
bool isTrackEmpty(const libmusictok::TrackType &track);

} // namespace Internal

} // namespace libmusictokUtils
