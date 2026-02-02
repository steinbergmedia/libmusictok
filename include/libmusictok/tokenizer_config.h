//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/tokenizer_config.h
// Created by  : Steinberg, 02/2025
// Description : TokenizerConfig class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/constants.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace libmusictok {

class TokenizerConfig
{
public:
    // Constructor with defaults
    TokenizerConfig(
        std::pair<int, int> pitch_range_             = pitchRange_default,
        std::map<std::pair<int, int>, int> beat_res_ = beatRes_default, int num_velocities_ = numVelocities_default,
        bool remove_duplicated_notes_                   = removeDuplicatedNotes_default,
        const std::vector<std::string> &special_tokens_ = specialTokens_default,
        std::string encode_ids_split_ = encodeIdsSplit_default, bool use_velocities_ = useVelocities_default,
        const std::vector<int> &use_note_duration_programs_ = useNoteDurationPrograms_default,
        bool use_chords_ = useChords_default, bool use_rests_ = useRests_default, bool use_tempos_ = useTempos_default,
        bool use_time_signatures_ = useTimeSignature_default, bool use_sustain_pedals_ = useSustainPedals_default,
        bool use_pitch_bends_ = usePitchBends_default, bool use_programs_ = usePrograms_default,
        bool use_pitch_intervals_ = usePitchIntervals_default, bool use_pitchdrum_tokens_ = usePitchdrumTokens_default,
        double default_note_duration_                                     = defaultNoteDuration_default,
        std::map<std::pair<int, int>, int> beat_res_rest_                 = beatResRest_default,
        std::vector<std::pair<std::string, std::vector<int>>> chord_maps_ = chordMaps_default,
        bool chord_tokens_with_root_note_                                 = chordTokensWithRootNote_default,
        std::vector<int> chord_unknown_ = chordUnknown_default, int num_tempos_ = numTempos_default,
        std::pair<int, int> tempo_range_ = tempoRange_default, bool log_tempos_ = logTempos_default,
        bool delete_equal_successive_tempo_changes_ = deleteEqualSuccessiveTempoChanges_default,
        const std::vector<std::pair<int, std::vector<int>>> &time_signature_range_ = timeSignatureRange_default,
        bool sustain_pedal_duration_                                               = sustainPedalDuration_default,
        std::tuple<int, int, int> pitch_bend_range_                                = pitchBendRange_default,
        bool delete_equal_successive_time_sig_changes_ = deleteEqualSuccessiveTimeSigChanges_default,
        const std::vector<int> &programs_              = programs_default,
        bool one_token_stream_for_programs_            = oneTokenStreamForPrograms_default,
        bool program_changes_ = programChanges_default, int max_pitch_interval_ = maxPitchInterval_default,
        int pitch_intervals_max_time_dist_     = pitchIntervalsMaxTimeDist_default,
        std::pair<int, int> drums_pitch_range_ = drumPitchRange_default,
        bool ac_polyphony_track_ = acPolyphonyTrack_default, bool ac_polyphony_bar_ = acPolyphonyBar_default,
        int ac_polyphony_min_ = acPolyphonyMin_default, int ac_polyphony_max_ = acPolyphonyMax_default,
        bool ac_pitch_class_bar_ = acPitchClassBar_default, bool ac_note_density_track_ = acNoteDensityTrack_default,
        int ac_note_density_track_min_           = acNoteDensityTrackMin_default,
        int ac_note_density_track_max_           = acNoteDensityTrackMax_default,
        bool ac_note_density_bar_                = acNoteDensityBar_default,
        int ac_note_density_bar_max_             = acNoteDensityBarMax_default,
        bool ac_note_duration_bar_               = acNoteDurationBar_default,
        bool ac_note_duration_track_             = acNoteDurationTrack_default,
        bool ac_repetition_track_                = acRepetitionTrack_default,
        int ac_repetition_track_num_bins_        = acRepetitionTrackNumBins_default,
        int ac_repetition_track_num_consec_bars_ = acRepetitionTrackNumConsecBars_default,
        const std::map<std::string, nlohmann::ordered_json> &additional_params_ = {});

    ///< Copy method (performs deep copy)
    TokenizerConfig copy() const;

    ///< Returns maximum value in beat_res.
    int maxNumPosPerBeat() const;

    ///< Checks if using Duration tokens
    bool usingNoteDurationTokens() const;

    ///< Create an ordered_json object of this config
    nlohmann::ordered_json toDict() const;

    ///< Create a config from a json object
    static TokenizerConfig fromDict(const nlohmann::ordered_json jsonConfig);

    ///< Save to JSON file
    void saveToJson(const std::filesystem::path &out_path) const;

    ///< Load from JSON file
    static TokenizerConfig loadFromJson(const std::filesystem::path &configFilePath);

    // Equality operators. This shallowly compares first-level container sizes and values.
    bool operator==(const TokenizerConfig &other) const;
    bool operator!=(const TokenizerConfig &other) const;

    // Vars
    std::pair<int, int> pitchRange;
    std::map<std::pair<int, int>, int> beatRes;
    int numVelocities;
    bool removeDuplicatedNotes;
    std::string encodeIdsSplit;

    // Special tokens and additional tokens
    std::vector<std::string> specialTokens;

    // Additional token type parameters
    bool useVelocities;
    std::unordered_set<int> useNoteDurationPrograms;
    std::vector<int> orderedUseNoteDurationPrograms;
    bool useChords;
    bool useRests;
    bool useTempos;
    bool useTimeSignatures;
    bool useSustainPedals;
    bool usePitchBends;
    bool usePrograms;
    bool usePitchIntervals;
    bool usePitchdrumTokens;

    // Duration
    double defaultNoteDuration;

    // Programs
    std::unordered_set<int> programs;
    std::vector<int> orderedPrograms;
    bool oneTokenStreamForPrograms;
    bool programChanges;

    // Rests parameters
    std::map<std::pair<int, int>, int> beatResRest;

    // Chord parameters
    std::vector<std::pair<std::string, std::vector<int>>> chordMaps;
    bool chordTokensWithRootNote;
    std::vector<int> chordUnknown;

    // Tempo parameters
    int numTempos;
    std::pair<int, int> tempoRange;
    bool logTempos;
    bool deleteEqualSuccessiveTempoChanges;

    // Time signature parameters
    std::vector<std::pair<int, std::vector<int>>> timeSignatureRange;
    bool deleteEqualSuccessiveTimeSigChanges;

    // Sustain pedal parameter
    bool sustainPedalDuration;

    // Pitch bend parameters
    std::tuple<int, int, int> pitchBendRange;

    // Pitch interval parameters
    int maxPitchInterval;
    int pitchIntervalsMaxTimeDist;

    // Drums
    std::pair<int, int> drumsPitchRange;

    // Attribute control parameters
    bool acPolyphonyTrack;
    bool acPolyphonyBar;
    int acPolyphonyMin;
    int acPolyphonyMax;
    bool acPitchClassBar;
    bool acNoteDensityTrack;
    int acNoteDensityTrackMin;
    int acNoteDensityTrackMax;
    bool acNoteDensityBar;
    int acNoteDensityBarMax;
    bool acNoteDurationBar;
    bool acNoteDurationTrack;
    bool acRepetitionTrack;
    int acRepetitionTrackNumBins;
    int acRepetitionTrackNumConsecBars;

    // Additional parameters (any extra params)
    std::map<std::string, nlohmann::ordered_json> additionalParams;
};

//-----------------------------------------------------------
} // namespace libmusictok
