//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/constants.h
// Created by  : Steinberg, 02/2025
// Description : Defines project defaults
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace libmusictok {

// Versions – these should be set appropriately at build time.
inline const std::string CURRENT_MIDITOK_VERSION    = "unknown"; // set at build time
inline const std::string CURRENT_TOKENIZERS_VERSION = "unknown";
inline const std::string CURRENT_SYMUSIC_VERSION    = "unknown";

// File extensions
inline const std::set<std::string> MIDI_FILES_EXTENSIONS = {".mid", ".midi", ".MID", ".MIDI"};
inline const std::set<std::string> ABC_FILES_EXTENSIONS  = {".abc", ".ABC"};
// A union of the two sets
inline const std::set<std::string> SUPPORTED_MUSIC_FILE_EXTENSIONS = {".mid", ".midi", ".MID", ".MIDI", ".abc", ".ABC"};

inline const std::string DEFAULT_TOKENIZER_FILE_NAME = "tokenizer.json";

// For tokenization – use the first usable Unicode code point (after skipping 0–4 and 0x20)
static constexpr int CHR_ID_START = 33;

// TokenizerConfig/default preprocessing parameters:

// Recommended piano pitch range.
static constexpr std::pair<int, int> pitchRange_default = {21, 109};

// Beat resolution: mapping a (start, end) beat range to the number of samples per beat.
inline const std::map<std::pair<int, int>, int> beatRes_default = {
    { {0, 4}, 8},
    {{4, 12}, 4}
};

// Number of velocity bins (velocities 0 to 127 are quantized)
static constexpr int numVelocities_default = 32;

// Default special tokens
inline const std::string bosTokenName_default               = "BOS";
inline const std::string eosTokenName_default               = "EOS";
inline const std::string padTokenName_default               = "PAD";
inline const std::string maskTokenName_default              = "MASK";
inline const std::vector<std::string> specialTokens_default = {padTokenName_default, bosTokenName_default,
                                                               eosTokenName_default, maskTokenName_default};
// Mandatory special tokens
inline const std::vector<std::string> mandatorySpecialTokens_default = {"PAD"};

// Additional/Optional tokens flags
static constexpr bool useVelocities_default      = true;
static constexpr bool useChords_default          = false;
static constexpr bool useRests_default           = false;
static constexpr bool useTempos_default          = false;
static constexpr bool useTimeSignature_default   = false;
static constexpr bool useSustainPedals_default   = false;
static constexpr bool usePitchBends_default      = false;
static constexpr bool usePrograms_default        = false;
static constexpr bool usePitchdrumTokens_default = true;

// USE_NOTE_DURATION_PROGRAMS holds programs from -1 to 127.
inline const std::vector<int> useNoteDurationPrograms_default = []() {
    std::vector<int> v;
    for (int i = -1; i < 128; i++)
        v.push_back(i);

    return v;
}();

// Pitch intervals
static constexpr bool usePitchIntervals_default        = false;
static constexpr int maxPitchInterval_default          = 16;
static constexpr int pitchIntervalsMaxTimeDist_default = 1;

// Rest parameters: mapping beat ranges for rests
inline const std::map<std::pair<int, int>, int> beatResRest_default = {
    { {0, 1}, 8},
    { {1, 2}, 4},
    {{2, 12}, 2}
};

// Chord parameters

// Chord maps: known chord structures (root assumed 0)
// Note that inversions are not infered from here.
inline const std::vector<std::pair<std::string, std::vector<int>>> chordMaps_default = {
    {     "min",         {0, 3, 7}},
    {     "maj",         {0, 4, 7}},
    {     "dim",         {0, 3, 6}},
    {     "aug",         {0, 4, 8}},
    {    "sus2",         {0, 2, 7}},
    {    "sus4",         {0, 5, 7}},
    {    "7dom",     {0, 4, 7, 10}},
    {    "7min",     {0, 3, 7, 10}},
    {    "7maj",     {0, 4, 7, 11}},
    {"7halfdim",     {0, 3, 6, 10}},
    {    "7dim",      {0, 3, 6, 9}},
    {    "7aug",     {0, 4, 8, 11}},
    {    "9maj", {0, 4, 7, 10, 14}},
    {    "9min", {0, 4, 7, 10, 13}},
};

// Whether chord tokens include the root note in the token string.
static constexpr bool chordTokensWithRootNote_default = false;
// For chords that do not match, in MidiTok CHORD_UNKNOWN = None. Here we set it as an empty vector.
inline const std::vector<int> chordUnknown_default = {};
// Prefix used when building “unknown chord” tokens
inline const std::string unknownChordPrefix_default = "ukn";

// Tempo parameters
static constexpr int numTempos_default                          = 32;
static constexpr std::pair<int, int> tempoRange_default         = {40, 250};
static constexpr bool logTempos_default                         = false;
static constexpr bool deleteEqualSuccessiveTempoChanges_default = false;

// Time signature parameters.
// Mapping denominator to a list of possible numerators.
inline const std::vector<std::pair<int, std::vector<int>>> timeSignatureRange_default = {
    {8,         {3, 12, 6}},
    {4, {5, 6, 3, 2, 1, 4}}
};

// Sustain pedal parameter
static constexpr bool sustainPedalDuration_default = false;

// Pitch bend parameters: (min, max, num_bins)
static constexpr int pitchBendMin_default  = -8192;
static constexpr int pitchBendMax_default  = 8191;
static constexpr int pitchBendBins_default = 32;

// Create a tuple containing the pitch bend parameters
static constexpr std::tuple<int, int, int> pitchBendRange_default =
    std::make_tuple(pitchBendMin_default, pitchBendMax_default, pitchBendBins_default);

// Programs
// PROGRAMS: from -1 to 127.
inline const std::vector<int> programs_default = []() {
    std::vector<int> v;
    for (int i = -1; i < 128; i++)
        v.push_back(i);

    return v;
}();

// Flag: should programs be encoded in one token stream?
static constexpr bool oneTokenStreamForPrograms_default = true; // automatically set false when not using programs
// Flag: include program change events?
static constexpr bool programChanges_default = false;

// Drums: recommended pitch range for drums (GM2 specs)
inline const std::pair<int, int> drumPitchRange_default = {27, 88};

// Preprocessing flag
static constexpr bool removeDuplicatedNotes_default = false;

// Attribute controls (default arguments)
static constexpr bool acPolyphonyTrack_default    = false;
static constexpr bool acPolyphonyBar_default      = false;
static constexpr bool acPitchClassBar_default     = false;
static constexpr bool acNoteDensityTrack_default  = false;
static constexpr bool acNoteDensityBar_default    = false;
static constexpr bool acNoteDurationBar_default   = false;
static constexpr bool acNoteDurationTrack_default = false;
static constexpr bool acRepetitionTrack_default   = false;

static constexpr int acPolyphonyMin_default                 = 1;
static constexpr int acPolyphonyMax_default                 = 6;
static constexpr int acNoteDensityBarMax_default            = 18;
static constexpr int acNoteDensityTrackMin_default          = 0;
static constexpr int acNoteDensityTrackMax_default          = 18;
static constexpr int acRepetitionTrackNumConsecBars_default = 4;
static constexpr int acRepetitionTrackNumBins_default       = 10;

// Tokenizers–specific parameters
inline const std::set<std::string> mmmCompatibleTokenizers_default = {"TSD", "REMI", "MIDILike"};
static constexpr bool useBarEndTokens_default                      = false; // e.g. for REMI
static constexpr bool addTrailingBars_default                      = false; // e.g. for REMI

// Default values when writing new files
static constexpr int tempo_default                                = 120;
static constexpr std::pair<int, int> timeSignature_default        = {4, 4};
static constexpr int keySignatureKey_default                      = 0;
static constexpr int keySignatureTonalit_default                  = 0; // C major
static constexpr bool deleteEqualSuccessiveTimeSigChanges_default = false;
static constexpr int defaultVelocity_default                      = 100;
static constexpr double defaultNoteDuration_default               = 0.5;

// Tokenizer training parameters
inline const std::string defaultTrainingModelName_default      = "BPE";
inline const std::string encodeIdsSplit_default                = "bar";
inline int wordpieceMaxInputCharsPerWordBar_default            = 400;
static constexpr int wordpieceMaxInputCharsPerWordBeat_default = 100;
static constexpr int unigramMaxInputCharsPerWordBar_default    = 128;
static constexpr int unigramMaxInputCharsPerWordBeat_default   = 32;
inline const std::string unigramSpecialTokenSuffix_default     = "-unigram";

// For file splitting in datasets
static constexpr int maxNumFilesNumTokensPerNote_default = 200;

// Pitch classes for chords/tokens
inline const std::vector<std::string> pitchClasses_default = {"C",  "C#", "D",  "D#", "E",  "F",
                                                              "F#", "G",  "G#", "A",  "A#", "B"};

// Token types that should appear before pitch class tokens during tokenization
inline const std::vector<std::string> tokenTypeBeforePc_default = {"TimeSig", "Tempo"};

// ----------------------------------------------------------------
// Structures for instruments and instrument classes

struct Instrument
{
    std::string name;
    // store the pitch range as [min, max] inclusive.
    int pitch_min;
    int pitch_max;
};

struct InstrumentClass
{
    std::string name;
    // program range: [min, max] inclusive.
    int program_min;
    int program_max;
};

// Define the list of MIDI instruments.
// (For each “range(a, b)”, we assume the valid pitches are from a to b-1.)
inline const std::vector<Instrument> MIDI_INSTRUMENTS = {
    {                                      "Acoustic Grand Piano", 21, 108},
    {                                     "Bright Acoustic Piano", 21, 108},
    {                                      "Electric Grand Piano", 21, 108},
    {                                          "Honky-tonk Piano", 21, 108},
    {                                          "Electric Piano 1", 28, 103},
    {                                          "Electric Piano 2", 28, 103},
    {                                               "Harpsichord", 41,  89},
    {                                                     "Clavi", 36,  96},
    // Chromatic Percussion
    {                                                   "Celesta", 60, 108},
    {                                              "Glockenspiel", 72, 108},
    {                                                 "Music Box", 60,  84},
    {                                                "Vibraphone", 53,  89},
    {                                                   "Marimba", 48,  84},
    {                                                 "Xylophone", 65,  96},
    {                                             "Tubular Bells", 60,  77},
    {                                                  "Dulcimer", 60,  84},
    // Organs
    {                                             "Drawbar Organ", 36,  96},
    {                                          "Percussive Organ", 36,  96},
    {                                                "Rock Organ", 36,  96},
    {                                              "Church Organ", 21, 108},
    {                                                "Reed Organ", 36,  96},
    {                                                 "Accordion", 53,  89},
    {                                                 "Harmonica", 60,  84},
    {                                           "Tango Accordion", 53,  89},
    // Guitars
    {                                   "Acoustic Guitar (nylon)", 40,  84},
    {                                   "Acoustic Guitar (steel)", 40,  84},
    {                                    "Electric Guitar (jazz)", 40,  86},
    {                                   "Electric Guitar (clean)", 40,  86},
    {                                   "Electric Guitar (muted)", 40,  86},
    {                                         "Overdriven Guitar", 40,  86},
    {                                         "Distortion Guitar", 40,  86},
    {                                          "Guitar Harmonics", 40,  86},
    // Bass
    {                                             "Acoustic Bass", 28,  55},
    {                                    "Electric Bass (finger)", 28,  55},
    {                                      "Electric Bass (pick)", 28,  55},
    {                                             "Fretless Bass", 28,  55},
    {                                               "Slap Bass 1", 28,  55},
    {                                               "Slap Bass 2", 28,  55},
    {                                              "Synth Bass 1", 28,  55},
    {                                              "Synth Bass 2", 28,  55},
    // Strings & Orchestral instruments
    {                                                    "Violin", 55,  93},
    {                                                     "Viola", 48,  84},
    {                                                     "Cello", 36,  72},
    {                                                "Contrabass", 28,  55},
    {                                           "Tremolo Strings", 28,  93},
    {                                         "Pizzicato Strings", 28,  93},
    {                                           "Orchestral Harp", 23, 103},
    {                                                   "Timpani", 36,  57},
    // Ensembles
    {                                        "String Ensembles 1", 28,  96},
    {                                        "String Ensembles 2", 28,  96},
    {                                            "SynthStrings 1", 36,  96},
    {                                            "SynthStrings 2", 36,  96},
    {                                                "Choir Aahs", 48,  79},
    {                                                "Voice Oohs", 48,  79},
    {                                               "Synth Voice", 48,  84},
    {                                             "Orchestra Hit", 48,  72},
    // Brass
    {                                                   "Trumpet", 58,  94},
    {                                                  "Trombone", 34,  75},
    {                                                      "Tuba", 29,  55},
    {                                             "Muted Trumpet", 58,  82},
    {                                               "French Horn", 41,  77},
    {                                             "Brass Section", 36,  96},
    {                                             "Synth Brass 1", 36,  96},
    {                                             "Synth Brass 2", 36,  96},
    // Reed
    {                                               "Soprano Sax", 54,  87},
    {                                                  "Alto Sax", 49,  80},
    {                                                 "Tenor Sax", 42,  75},
    {                                              "Baritone Sax", 37,  68},
    {                                                      "Oboe", 58,  91},
    {                                              "English Horn", 52,  81},
    {                                                   "Bassoon", 34,  72},
    {                                                  "Clarinet", 50,  91},
    // Pipe
    {                                                   "Piccolo", 74, 108},
    {                                                     "Flute", 60,  96},
    {                                                  "Recorder", 60,  96},
    {                                                 "Pan Flute", 60,  96},
    {                                              "Blown Bottle", 60,  96},
    {                                                "Shakuhachi", 55,  84},
    {                                                   "Whistle", 60,  96},
    {                                                   "Ocarina", 60,  84},
    // Synth Lead
    {                                           "Lead 1 (square)", 21, 108},
    {                                         "Lead 2 (sawtooth)", 21, 108},
    {                                         "Lead 3 (calliope)", 36,  96},
    {                                            "Lead 4 (chiff)", 36,  96},
    {                                          "Lead 5 (charang)", 36,  96},
    {                                            "Lead 6 (voice)", 36,  96},
    {                                           "Lead 7 (fifths)", 36,  96},
    {                                      "Lead 8 (bass + lead)", 21, 108},
    // Synth Pad
    {                                           "Pad 1 (new age)", 36,  96},
    {                                              "Pad 2 (warm)", 36,  96},
    {                                         "Pad 3 (polysynth)", 36,  96},
    {                                             "Pad 4 (choir)", 36,  96},
    {                                             "Pad 5 (bowed)", 36,  96},
    {                                          "Pad 6 (metallic)", 36,  96},
    {                                              "Pad 7 (halo)", 36,  96},
    {                                             "Pad 8 (sweep)", 36,  96},
    // Synth SFX
    {                                               "FX 1 (rain)", 36,  96},
    {                                         "FX 2 (soundtrack)", 36,  96},
    {                                            "FX 3 (crystal)", 36,  96},
    {                                         "FX 4 (atmosphere)", 36,  96},
    {                                         "FX 5 (brightness)", 36,  96},
    {                                            "FX 6 (goblins)", 36,  96},
    {                                             "FX 7 (echoes)", 36,  96},
    {                                             "FX 8 (sci-fi)", 36,  96},
    // Ethnic Misc.
    {                                                     "Sitar", 48,  77},
    {                                                     "Banjo", 48,  84},
    {                                                  "Shamisen", 50,  79},
    {                                                      "Koto", 55,  84},
    {                                                   "Kalimba", 48,  79},
    {                                                  "Bag pipe", 36,  77},
    {                                                    "Fiddle", 55,  96},
    {                                                    "Shanai", 48,  72},
    // Percussive – for these instruments, range(128) means 0 to 127.
    {                                               "Tinkle Bell", 72,  84},
    {                                                     "Agogo", 60,  72},
    {                                               "Steel Drums", 52,  76},
    {                                                 "Woodblock",  0, 127},
    {                                                "Taiko Drum",  0, 127},
    {                                               "Melodic Tom",  0, 127},
    {                                                "Synth Drum",  0, 127},
    {                                            "Reverse Cymbal",  0, 127},
    // SFX
    {                   "Guitar Fret Noise, Guitar Cutting Noise",  0, 127},
    {                             "Breath Noise, Flute Key Click",  0, 127},
    {            "Seashore, Rain, Thunder, Wind, Stream, Bubbles",  0, 127},
    {                             "Bird Tweet, Dog, Horse Gallop",  0, 127},
    {  "Telephone Ring, Door Creaking, Door, Scratch, Wind Chime",  0, 127},
    {                                    "Helicopter, Car Sounds",  0, 127},
    {"Applause, Laughing, Screaming, Punch, Heart Beat, Footstep",  0, 127},
    {                 "Gunshot, Machine Gun, Lasergun, Explosion",  0, 127},
};

// Define instrument classes. For each, we store the program range as [min, max] inclusive.
inline const std::vector<InstrumentClass> INSTRUMENT_CLASSES = {
    {               "Piano",   0,   7}, // range(8)
    {"Chromatic Percussion",   8,  15}, // range(8,16)
    {               "Organ",  16,  23}, // range(16,24)
    {              "Guitar",  24,  31}, // range(24,32)
    {                "Bass",  32,  39}, // range(32,40)
    {             "Strings",  40,  47}, // range(40,48)
    {            "Ensemble",  48,  55}, // range(48,56)
    {               "Brass",  56,  63}, // range(56,64)
    {                "Reed",  64,  71}, // range(64,72)
    {                "Pipe",  72,  79}, // range(72,80)
    {          "Synth Lead",  80,  87}, // range(80,88)
    {           "Synth Pad",  88,  95}, // range(88,96)
    {       "Synth Effects",  96, 103}, // range(96,104)
    {              "Ethnic", 104, 111}, // range(104,112)
    {          "Percussive", 112, 119}, // range(112,120)
    {       "Sound Effects", 120, 127}, // range(120,128)
    {               "Drums",  -1,  -1}  // range(-1,0)
};

// DRUM_SETS: mapping of drum set key to name.
inline const std::map<int, std::string> DRUM_SETS = {
    { 0,   "Standard"},
    { 8,       "Room"},
    {16,      "Power"},
    {24, "Electronic"},
    {25,     "Analog"},
    {32,       "Jazz"},
    {40,      "Brush"},
    {48,  "Orchestra"},
    {56,        "SFX"}
};

// Control changes mapping (from MIDI Control Change numbers to descriptive names)
inline const std::map<int, std::string> CONTROL_CHANGES = {
    // MSB
    {  0,                             "Bank Select"},
    {  1,                        "Modulation Depth"},
    {  2,                       "Breath Controller"},
    {  4,                         "Foot Controller"},
    {  5,                         "Portamento Time"},
    {  6,                              "Data Entry"},
    {  7,                          "Channel Volume"},
    {  8,                                 "Balance"},
    { 10,                                     "Pan"},
    { 11,                   "Expression Controller"},
    // LSB
    { 32,                             "Bank Select"},
    { 33,                        "Modulation Depth"},
    { 34,                       "Breath Controller"},
    { 36,                         "Foot Controller"},
    { 37,                         "Portamento Time"},
    { 38,                              "Data Entry"},
    { 39,                          "Channel Volume"},
    { 40,                                 "Balance"},
    { 42,                                     "Pan"},
    { 43,                   "Expression Controller"},
    // On/Off controls (≤63 off, ≥64 on)
    { 64,                            "Damper Pedal"},
    { 65,                              "Portamento"},
    { 66,                               "Sostenuto"},
    { 67,                              "Soft Pedal"},
    { 68,                       "Legato Footswitch"},
    { 69,                                  "Hold 2"},
    // Continuous controls
    { 70,                         "Sound Variation"},
    { 71,               "Timbre/Harmonic Intensity"},
    { 72,                            "Release Time"},
    { 73,                             "Attack Time"},
    { 74,                              "Brightness"},
    { 75,                              "Decay Time"},
    { 76,                            "Vibrato Rate"},
    { 77,                           "Vibrato Depth"},
    { 78,                           "Vibrato Delay"},
    { 84,                      "Portamento Control"},
    { 88,         "High Resolution Velocity Prefix"},
    // Effects depths
    { 91,                            "Reverb Depth"},
    { 92,                           "Tremolo Depth"},
    { 93,                            "Chorus Depth"},
    { 94,                           "Celeste Depth"},
    { 95,                            "Phaser Depth"},
    // Registered parameter numbers
    { 96,                          "Data Increment"},
    { 97,                          "Data Decrement"},
    {100, "Registered Parameter Number (RPN) - LSB"},
    {101, "Registered Parameter Number (RPN) - MSB"},
    // Channel mode controls
    {120,                           "All Sound Off"},
    {121,                   "Reset All Controllers"},
    {122,                    "Local Control On/Off"},
    {123,                           "All Notes Off"},
    {124,                           "Omni Mode Off"},
    {125,                            "Omni Mode On"},
    {126,                            "Mono Mode On"},
    {127,                            "Poly Mode On"}
};

} // namespace libmusictok
