//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/tokenizer_config.cpp
// Created by  : Steinberg, 02/2025
// Description : TokenizerConfig class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizer_config.h"
#include "libmusictok/utility_functions.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
std::string formatSpecialToken(const std::string &token)
{
    // Split the token using '_' as a delimiter.
    std::vector<std::string> parts = libmusictokUtils::split(token, "_");

    if (parts.size() == 1)
    {
        // If there is only one part, append "None".
        parts.push_back("None");
    }
    else if (parts.size() > 2)
    {
        // If there are more than two parts, join all but the last with '-' and warn.
        std::string combined;
        for (size_t i = 0; i < parts.size() - 1; ++i)
        {
            if (i > 0)
            {
                combined += "-";
            }
            combined += parts[i];
        }
        std::vector<std::string> newParts = {combined, parts.back()};
        std::clog << "Warning: libmusictok::TokenizerConfig: special token " << token
                  << " must contain one underscore (_). This token will be saved as "
                  << libmusictokUtils::join(newParts, "_") << "." << std::endl;
        parts = newParts;
    }
    return libmusictokUtils::join(parts, "_");
}

//------------------------------------------------------------------------
TokenizerConfig::TokenizerConfig(
    std::pair<int, int> pitch_range_, std::map<std::pair<int, int>, int> beat_res_, int num_velocities_,
    bool remove_duplicated_notes_, const std::vector<std::string> &special_tokens_, std::string encode_ids_split_,
    bool use_velocities_, const std::vector<int> &use_note_duration_programs_, bool use_chords_, bool use_rests_,
    bool use_tempos_, bool use_time_signatures_, bool use_sustain_pedals_, bool use_pitch_bends_, bool use_programs_,
    bool use_pitch_intervals_, bool use_pitchdrum_tokens_, double default_note_duration_,
    std::map<std::pair<int, int>, int> beat_res_rest_,
    std::vector<std::pair<std::string, std::vector<int>>> chord_maps_, bool chord_tokens_with_root_note_,
    std::vector<int> chord_unknown_, int num_tempos_, std::pair<int, int> tempo_range_, bool log_tempos_,
    bool delete_equal_successive_tempo_changes_,
    const std::vector<std::pair<int, std::vector<int>>> &time_signature_range_, bool sustain_pedal_duration_,
    std::tuple<int, int, int> pitch_bend_range_, bool delete_equal_successive_time_sig_changes_,
    const std::vector<int> &programs_, bool one_token_stream_for_programs_, bool program_changes_,
    int max_pitch_interval_, int pitch_intervals_max_time_dist_, std::pair<int, int> drums_pitch_range_,
    bool ac_polyphony_track_, bool ac_polyphony_bar_, int ac_polyphony_min_, int ac_polyphony_max_,
    bool ac_pitch_class_bar_, bool ac_note_density_track_, int ac_note_density_track_min_,
    int ac_note_density_track_max_, bool ac_note_density_bar_, int ac_note_density_bar_max_, bool ac_note_duration_bar_,
    bool ac_note_duration_track_, bool ac_repetition_track_, int ac_repetition_track_num_bins_,
    int ac_repetition_track_num_consec_bars_, const std::map<std::string, nlohmann::ordered_json> &additional_params_)
    : pitchRange(pitch_range_)
    , beatRes(std::move(beat_res_))
    , numVelocities(num_velocities_)
    , removeDuplicatedNotes(remove_duplicated_notes_)
    , encodeIdsSplit(encode_ids_split_)
    , useVelocities(use_velocities_)
    , useChords(use_chords_)
    , useRests(use_rests_)
    , useTempos(use_tempos_)
    , useTimeSignatures(use_time_signatures_)
    , useSustainPedals(use_sustain_pedals_)
    , usePitchBends(use_pitch_bends_)
    , usePrograms(use_programs_)
    , usePitchIntervals(use_pitch_intervals_)
    , usePitchdrumTokens(use_pitchdrum_tokens_)
    , defaultNoteDuration(default_note_duration_)
    , oneTokenStreamForPrograms(one_token_stream_for_programs_ && use_programs_)
    , programChanges(program_changes_ && use_programs_)
    , beatResRest(std::move(beat_res_rest_))
    , chordTokensWithRootNote(chord_tokens_with_root_note_)
    , chordUnknown(chord_unknown_)
    , numTempos(num_tempos_)
    , tempoRange(tempo_range_)
    , logTempos(log_tempos_)
    , deleteEqualSuccessiveTempoChanges(delete_equal_successive_tempo_changes_)
    , deleteEqualSuccessiveTimeSigChanges(delete_equal_successive_time_sig_changes_)
    , sustainPedalDuration(sustain_pedal_duration_ && use_sustain_pedals_)
    , pitchBendRange(pitch_bend_range_)
    , maxPitchInterval(max_pitch_interval_)
    , pitchIntervalsMaxTimeDist(pitch_intervals_max_time_dist_)
    , drumsPitchRange(drums_pitch_range_)
    , acPolyphonyTrack(ac_polyphony_track_)
    , acPolyphonyBar(ac_polyphony_bar_)
    , acPolyphonyMin(ac_polyphony_min_)
    , acPolyphonyMax(ac_polyphony_max_)
    , acPitchClassBar(ac_pitch_class_bar_)
    , acNoteDensityTrack(ac_note_density_track_)
    , acNoteDensityTrackMin(ac_note_density_track_min_)
    , acNoteDensityTrackMax(ac_note_density_track_max_)
    , acNoteDensityBar(ac_note_density_bar_)
    , acNoteDensityBarMax(ac_note_density_bar_max_)
    , acNoteDurationBar(ac_note_duration_bar_)
    , acNoteDurationTrack(ac_note_duration_track_)
    , acRepetitionTrack(ac_repetition_track_)
    , acRepetitionTrackNumBins(ac_repetition_track_num_bins_)
    , acRepetitionTrackNumConsecBars(ac_repetition_track_num_consec_bars_)
    , additionalParams(additional_params_)
{
    // --- Checks ---
    if (!(0 <= pitchRange.first && pitchRange.first < pitchRange.second && pitchRange.second <= 127))
    {
        std::ostringstream oss;
        oss << "`pitch_range` must be within 0 and 127, and the first value "
            << "less than the second (received (" << pitchRange.first << ", " << pitchRange.second << "))";
        throw std::invalid_argument(oss.str());
    }
    if (!(1 <= numVelocities && numVelocities <= 127))
    {
        std::ostringstream oss;
        oss << "`num_velocities` must be within 1 and 127 (received " << numVelocities << ")";
        throw std::invalid_argument(oss.str());
    }
    if (maxPitchInterval && !(0 <= maxPitchInterval && maxPitchInterval <= 127))
    {
        std::ostringstream oss;
        oss << "`max_pitch_interval` must be within 0 and 127 (received " << maxPitchInterval << ")";
        throw std::invalid_argument(oss.str());
    }
    if (useTimeSignatures)
    {
        // Validate that denominators are powers of 2.
        for (const auto &[denom, numerators] : time_signature_range_)
        {
            double log_val = std::log2(denom);
            if (std::fabs(log_val - std::round(log_val)) > 1e-6)
            {
                std::ostringstream oss;
                oss << "`time_signature_range` contains an invalid denominator " << denom
                    << ". Only powers of 2 denominators are supported.";
                throw std::invalid_argument(oss.str());
            }
        }
    }

    // Global parameters already set by initializer list.

    // Special tokens processing:
    for (const auto &token : special_tokens_)
    {
        std::string formatted = formatSpecialToken(token);
        if (std::find(this->specialTokens.begin(), this->specialTokens.end(), formatted) == this->specialTokens.end())
        {
            this->specialTokens.push_back(formatted);
        }
        else
        {
            std::clog << "Warning: The special token " << formatted
                      << " is present twice in your configuration. Skipping duplicate." << std::endl;
        }
    }
    // Add mandatory tokens (without warning)
    for (const auto &token : mandatorySpecialTokens_default)
    {
        std::string formatted = formatSpecialToken(token);
        if (std::find(this->specialTokens.begin(), this->specialTokens.end(), formatted) == this->specialTokens.end())
        {
            this->specialTokens.push_back(formatted);
        }
    }

    // Additional token types params:
    for (const auto &prog : use_note_duration_programs_)
    {
        if (useNoteDurationPrograms.find(prog) == useNoteDurationPrograms.end())
        {
            // maintain a separate list here for ordered values for creating the vocab when needed...
            orderedUseNoteDurationPrograms.push_back(prog);
            useNoteDurationPrograms.insert(prog);
        }
    }

    // Programs:
    for (const auto &prog : programs_)
    {
        if (programs.find(prog) == programs.end())
        {
            // maintain a separate list here for ordered values for creating the vocab when needed...
            orderedPrograms.push_back(prog);
            programs.insert(prog);
        }
    }

    // Check for rest compatibility with duration tokens:
    if (this->useRests && this->useNoteDurationPrograms.size() < 129)
    {
        std::string msg =
            "Disabling rest tokens. `Rest` tokens are compatible only when note duration tokens are enabled.";
        if (!usePrograms)
        {
            this->useRests = false;
            std::clog << "Warning: " << msg << " Your configuration disables `Program` tokens." << std::endl;
        }
        else
        {
            bool flag = false;
            for (auto p : this->programs)
            {
                if (this->useNoteDurationPrograms.find(p) == this->useNoteDurationPrograms.end())
                {
                    flag = true;
                    break;
                }
            }
            if (flag)
            {
                this->useRests = false;
                std::clog << "Warning: " << msg << " Note duration tokens not enabled for all programs." << std::endl;
            }
        }
    }

    // Check for beat_res_rest maximum resolution:
    if (useRests)
    {
        int maxRestRes = 0;
        for (const auto &kv : this->beatResRest)
        {
            if (kv.second > maxRestRes)
                maxRestRes = kv.second;
        }
        int maxGlobalRes = 0;
        for (const auto &kv : this->beatRes)
        {
            if (kv.second > maxGlobalRes)
                maxGlobalRes = kv.second;
        }
        if (maxRestRes > maxGlobalRes)
        {
            std::ostringstream oss;
            oss << "The maximum resolution of the rests (" << maxRestRes
                << ") must be <= the maximum resolution of the global beat resolution (" << maxGlobalRes << ").";
            throw std::invalid_argument(oss.str());
        }
    }

    this->timeSignatureRange = time_signature_range_;
    this->chordMaps          = chord_maps_;

    // Attribute controls are set by initializer list.
}

//------------------------------------------------------------------------
TokenizerConfig TokenizerConfig::copy() const
{
    return *this;
}

//------------------------------------------------------------------------
int TokenizerConfig::maxNumPosPerBeat() const
{
    int maxVal = 0;
    for (const auto &kv : beatRes)
    {
        if (kv.second > maxVal)
            maxVal = kv.second;
    }
    return maxVal;
}

//------------------------------------------------------------------------
bool TokenizerConfig::usingNoteDurationTokens() const
{
    return !useNoteDurationPrograms.empty();
}

//------------------------------------------------------------------------
nlohmann::ordered_json TokenizerConfig::toDict() const
{
    nlohmann::ordered_json j;
    j["pitch_range"] = {pitchRange.first, pitchRange.second};

    // Convert map with pair keys to a simple key string with format "x_y"
    nlohmann::ordered_json beatResJson;
    for (const auto &kv : beatRes)
    {
        beatResJson[std::to_string(kv.first.first) + "_" + std::to_string(kv.first.second)] = kv.second;
    }
    j["beat_res"] = beatResJson;

    j["num_velocities"]          = numVelocities;
    j["remove_duplicated_notes"] = removeDuplicatedNotes;
    j["encode_ids_split"]        = encodeIdsSplit;
    j["special_tokens"]          = specialTokens;
    j["use_velocities"]          = useVelocities;
    j["use_note_duration_programs"] =
        std::vector<int>(orderedUseNoteDurationPrograms.begin(), orderedUseNoteDurationPrograms.end());
    j["use_chords"]            = useChords;
    j["use_rests"]             = useRests;
    j["use_tempos"]            = useTempos;
    j["use_time_signatures"]   = useTimeSignatures;
    j["use_sustain_pedals"]    = useSustainPedals;
    j["use_pitch_bends"]       = usePitchBends;
    j["use_programs"]          = usePrograms;
    j["use_pitch_intervals"]   = usePitchIntervals;
    j["use_pitchdrum_tokens"]  = usePitchdrumTokens;
    j["default_note_duration"] = defaultNoteDuration;
    j["programs"]              = std::vector<int>(orderedPrograms.begin(), orderedPrograms.end());

    j["one_token_stream_for_programs"] = oneTokenStreamForPrograms;
    j["program_changes"]               = programChanges;

    // Serialize beat_res_rest
    nlohmann::ordered_json beatResRestJson;
    for (const auto &kv : beatResRest)
    {
        beatResRestJson[std::to_string(kv.first.first) + "_" + std::to_string(kv.first.second)] = kv.second;
    }
    j["beat_res_rest"] = beatResRestJson;

    // Chord maps
    nlohmann::ordered_json chordMapsJson;
    for (const auto &kv : chordMaps)
    {
        const auto &vec         = kv.second;
        chordMapsJson[kv.first] = vec; // Directly assign the vector to the JSON object
    }
    j["chord_maps"]                  = chordMapsJson;
    j["chord_tokens_with_root_note"] = chordTokensWithRootNote;

    if (!chordUnknown.empty())
    {
        j["chord_unknown"] = chordUnknown;
    }
    else
    {
        j["chord_unknown"] = nullptr;
    }

    j["num_tempos"]                            = numTempos;
    j["tempo_range"]                           = {tempoRange.first, tempoRange.second};
    j["log_tempos"]                            = logTempos;
    j["delete_equal_successive_tempo_changes"] = deleteEqualSuccessiveTempoChanges;

    // Time signature range:
    nlohmann::ordered_json timeSigJson;
    for (const auto &kv : timeSignatureRange)
    {
        timeSigJson[std::to_string(kv.first)] = kv.second;
    }
    j["time_signature_range"]                     = timeSigJson;
    j["delete_equal_successive_time_sig_changes"] = deleteEqualSuccessiveTimeSigChanges;
    j["sustain_pedal_duration"]                   = sustainPedalDuration;

    // Pack pitch_bend_range tuple:
    nlohmann::ordered_json pbj = {std::get<0>(pitchBendRange), std::get<1>(pitchBendRange),
                                  std::get<2>(pitchBendRange)};
    j["pitch_bend_range"]      = pbj;

    j["max_pitch_interval"]                  = maxPitchInterval;
    j["pitch_intervals_max_time_dist"]       = pitchIntervalsMaxTimeDist;
    j["drums_pitch_range"]                   = {drumsPitchRange.first, drumsPitchRange.second};
    j["ac_polyphony_track"]                  = acPolyphonyTrack;
    j["ac_polyphony_bar"]                    = acPolyphonyBar;
    j["ac_polyphony_min"]                    = acPolyphonyMin;
    j["ac_polyphony_max"]                    = acPolyphonyMax;
    j["ac_pitch_class_bar"]                  = acPitchClassBar;
    j["ac_note_density_track"]               = acNoteDensityTrack;
    j["ac_note_density_track_min"]           = acNoteDensityTrackMin;
    j["ac_note_density_track_max"]           = acNoteDensityTrackMax;
    j["ac_note_density_bar"]                 = acNoteDensityBar;
    j["ac_note_density_bar_max"]             = acNoteDensityBarMax;
    j["ac_note_duration_bar"]                = acNoteDurationBar;
    j["ac_note_duration_track"]              = acNoteDurationTrack;
    j["ac_repetition_track"]                 = acRepetitionTrack;
    j["ac_repetition_track_num_bins"]        = acRepetitionTrackNumBins;
    j["ac_repetition_track_num_consec_bars"] = acRepetitionTrackNumConsecBars;

    j["additional_params"] = additionalParams;

    return j;
}

//------------------------------------------------------------------------
TokenizerConfig TokenizerConfig::fromDict(nlohmann::ordered_json jsonConfig)
{
    // Convert beat_res and beat_res_rest keys from strings "x_y" to pairs.
    std::map<std::pair<int, int>, int> beatResLoaded;
    for (auto it = jsonConfig["beat_res"].begin(); it != jsonConfig["beat_res"].end(); ++it)
    {
        std::string key                = it.key();
        size_t pos                     = key.find('_');
        int first                      = std::stoi(key.substr(0, pos));
        int second                     = std::stoi(key.substr(pos + 1));
        beatResLoaded[{first, second}] = it.value();
    }

    std::map<std::pair<int, int>, int> beatResRestLoaded;
    for (auto it = jsonConfig["beat_res_rest"].begin(); it != jsonConfig["beat_res_rest"].end(); ++it)
    {
        std::string key                    = it.key();
        size_t pos                         = key.find('_');
        int first                          = std::stoi(key.substr(0, pos));
        int second                         = std::stoi(key.substr(pos + 1));
        beatResRestLoaded[{first, second}] = it.value();
    }

    // remap special_tokens
    std::vector<std::string> specialTokensLoaded = jsonConfig["special_tokens"];
    jsonConfig["special_tokens"]                 = specialTokensLoaded;

    // Convert time_signature_range keys into int keys.
    std::vector<std::pair<int, std::vector<int>>> tsrange;
    for (auto it = jsonConfig["time_signature_range"].begin(); it != jsonConfig["time_signature_range"].end(); ++it)
    {
        int denom = std::stoi(it.key());
        tsrange.emplace_back(denom, it.value().get<std::vector<int>>());
    }

    // fix up chordMaps
    std::vector<std::pair<std::string, std::vector<int>>> chordMapsLoaded;
    for (auto it = jsonConfig["chord_maps"].begin(); it != jsonConfig["chord_maps"].end(); ++it)
    {
        std::string chordName = it.key();
        chordMapsLoaded.emplace_back(chordName, it.value().get<std::vector<int>>());
    }

    std::vector<int> chordUnknown_;
    if (jsonConfig["chord_unknown"].is_null())
    {
        chordUnknown_ = chordUnknown_default;
    }
    else
    {
        chordUnknown_ = jsonConfig["chord_unknown"].get<std::vector<int>>();
    }

    // Now, construct a TokenizerConfig
    // Note: We assume additional_params exists.
    auto config = TokenizerConfig(
        jsonConfig.value("pitch_range", nlohmann::json::array({pitchRange_default.first, pitchRange_default.second}))
                    .size() >= 2
            ? std::make_pair(jsonConfig["pitch_range"][0].get<int>(), jsonConfig["pitch_range"][1].get<int>())
            : pitchRange_default,
        beatResLoaded, jsonConfig.value("num_velocities", numVelocities_default),
        jsonConfig.value("remove_duplicated_notes", removeDuplicatedNotes_default),
        jsonConfig.value("special_tokens", specialTokens_default),
        jsonConfig.value("encode_ids_split", encodeIdsSplit_default),
        jsonConfig.value("use_velocities", useVelocities_default),
        jsonConfig.value("use_note_duration_programs", useNoteDurationPrograms_default),
        jsonConfig.value("use_chords", useChords_default), jsonConfig.value("use_rests", useRests_default),
        jsonConfig.value("use_tempos", useTempos_default),
        jsonConfig.value("use_time_signatures", useTimeSignature_default),
        jsonConfig.value("use_sustain_pedals", useSustainPedals_default),
        jsonConfig.value("use_pitch_bends", usePitchBends_default),
        jsonConfig.value("use_programs", usePrograms_default),
        jsonConfig.value("use_pitch_intervals", usePitchIntervals_default),
        jsonConfig.value("use_pitchdrum_tokens", usePitchdrumTokens_default),
        jsonConfig.value("default_note_duration", defaultNoteDuration_default), beatResRestLoaded, chordMapsLoaded,
        jsonConfig.value("chord_tokens_with_root_note", chordTokensWithRootNote_default), chordUnknown_,
        jsonConfig.value("num_tempos", numTempos_default),
        jsonConfig.value("tempo_range", nlohmann::json::array({tempoRange_default.first, tempoRange_default.second}))
                    .size() >= 2
            ? std::make_pair(jsonConfig["tempo_range"][0].get<int>(), jsonConfig["tempo_range"][1].get<int>())
            : tempoRange_default,
        jsonConfig.value("log_tempos", logTempos_default),
        jsonConfig.value("delete_equal_successive_tempo_changes", deleteEqualSuccessiveTempoChanges_default), tsrange,
        jsonConfig.value("sustain_pedal_duration", sustainPedalDuration_default),
        jsonConfig.value("pitch_bend_range", pitchBendRange_default),
        jsonConfig.value("delete_equal_successive_time_sig_changes", deleteEqualSuccessiveTimeSigChanges_default),
        jsonConfig.value("programs", programs_default),
        jsonConfig.value("one_token_stream_for_programs", oneTokenStreamForPrograms_default),
        jsonConfig.value("program_changes", programChanges_default),
        jsonConfig.value("max_pitch_interval", maxPitchInterval_default),
        jsonConfig.value("pitch_intervals_max_time_dist", pitchIntervalsMaxTimeDist_default),
        jsonConfig.value("drums_pitch_range", drumPitchRange_default),
        jsonConfig.value("ac_polyphony_track", acPolyphonyTrack_default),
        jsonConfig.value("ac_polyphony_bar", acPolyphonyBar_default),
        jsonConfig.value("ac_polyphony_min", acPolyphonyMin_default),
        jsonConfig.value("ac_polyphony_max", acPolyphonyMax_default),
        jsonConfig.value("ac_pitch_class_bar", acPitchClassBar_default),
        jsonConfig.value("ac_note_density_track", acNoteDensityTrack_default),
        jsonConfig.value("ac_note_density_track_min", acNoteDensityTrackMin_default),
        jsonConfig.value("ac_note_density_track_max", acNoteDensityTrackMax_default),
        jsonConfig.value("ac_note_density_bar", acNoteDensityBar_default),
        jsonConfig.value("ac_note_density_bar_max", acNoteDensityBarMax_default),
        jsonConfig.value("ac_note_duration_bar", acNoteDurationBar_default),
        jsonConfig.value("ac_note_duration_track", acNoteDurationTrack_default),
        jsonConfig.value("ac_repetition_track", acRepetitionTrack_default),
        jsonConfig.value("ac_repetition_track_num_bins", acRepetitionTrackNumBins_default),
        jsonConfig.value("ac_repetition_track_num_consec_bars", acRepetitionTrackNumConsecBars_default),
        jsonConfig.value("additional_params", std::map<std::string, nlohmann::ordered_json>()));

    return config;
}

//------------------------------------------------------------------------
void TokenizerConfig::saveToJson(const std::filesystem::path &out_path) const
{
    std::filesystem::create_directories(out_path.parent_path());
    std::ofstream outfile(out_path);
    if (!outfile)
        throw std::runtime_error("Unable to open file: " + out_path.string());
    nlohmann::ordered_json j = toDict();
    outfile << j.dump(4);
}

//------------------------------------------------------------------------
TokenizerConfig TokenizerConfig::loadFromJson(const std::filesystem::path &configFilePath)
{
    nlohmann::ordered_json jsonConfig = libmusictokUtils::loadOrderedJson(configFilePath);
    return fromDict(jsonConfig);
}

//------------------------------------------------------------------------
bool TokenizerConfig::operator==(const TokenizerConfig &other) const
{
    // For each key in to_dict, compare with other's to_dict.
    nlohmann::ordered_json j1 = this->toDict();
    nlohmann::ordered_json j2 = other.toDict();
    return j1 == j2;
}

//------------------------------------------------------------------------
bool TokenizerConfig::operator!=(const TokenizerConfig &other) const
{
    return !(*this == other);
}
} // namespace libmusictok
