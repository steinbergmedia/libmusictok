//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/testing_utilities.hpp
// Created by  : Steinberg, 09/2025
// Description : Helper functions to the testing suite
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/tokenizations/cpword.h"
#include "libmusictok/tokenizations/midilike.h"
#include "libmusictok/tokenizations/mmm.h"
#include "libmusictok/tokenizations/mumidi.h"
#include "libmusictok/tokenizations/music_tokenizer.h"
#include "libmusictok/tokenizations/octuple.h"
#include "libmusictok/tokenizations/pertok.h"
#include "libmusictok/tokenizations/remi.h"
#include "libmusictok/tokenizations/structured.h"
#include "libmusictok/tokenizations/tsd.h"
#include "libmusictok/tokenizer_config.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

using namespace libmusictok;
namespace libmusictokTest {

// constants
constexpr int maxBarEmbedding_default      = 2000;
inline const std::string projrectSourceDir = PROJECT_SOURCE_DIR;
inline const std::string resourcesPath     = RESOURCES_DIRECTORY;

//------------------------------------------------------------------------
inline std::vector<std::string> getAllTokenizations()
{
    return {"REMI", "TSD", "MIDILike", "MMM", "CPWord", "MuMIDI", "Structured", "Octuple", "PerTok"};
}

//------------------------------------------------------------------------
inline std::vector<std::string> getMMMBaseTokenizations()
{
    return {"REMI", "TSD", "MIDILike"};
}

//------------------------------------------------------------------------
inline std::vector<std::filesystem::path> getMidiPathsMultitrack()
{
    std::vector<std::filesystem::path> result;
    std::filesystem::path midiFolder = std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs";
    for (auto &p : std::filesystem::recursive_directory_iterator(midiFolder))
    {
        if (p.is_regular_file() && p.path().extension() == ".mid")
        {
            result.push_back(p.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

//------------------------------------------------------------------------
inline std::vector<std::filesystem::path> getMidiPathsOneTrack()
{
    std::vector<std::filesystem::path> result;
    std::filesystem::path midiFolder = std::filesystem::path(resourcesPath) / "scores" / "One_track_MIDIs";
    for (auto &p : std::filesystem::recursive_directory_iterator(midiFolder))
    {
        if (p.is_regular_file() && p.path().extension() == ".mid")
        {
            result.push_back(p.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

//------------------------------------------------------------------------
inline std::vector<std::filesystem::path> getMidiPathsCorrupted()
{
    std::vector<std::filesystem::path> result;
    std::filesystem::path midiFolder = std::filesystem::path(resourcesPath) / "scores" / "MIDIs_corrupted";
    for (auto &p : std::filesystem::recursive_directory_iterator(midiFolder))
    {
        if (p.is_regular_file() && p.path().extension() == ".mid")
        {
            result.push_back(p.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

//------------------------------------------------------------------------
inline std::vector<std::filesystem::path> getAllMidiPaths()
{
    std::vector<std::filesystem::path> result;
    std::filesystem::path multiTrackFolder = std::filesystem::path(resourcesPath) / "scores" / "Multitrack_MIDIs";
    for (auto &p : std::filesystem::recursive_directory_iterator(multiTrackFolder))
    {
        if (p.is_regular_file() && p.path().extension() == ".mid")
        {
            result.push_back(p.path());
        }
    }
    std::filesystem::path oneTrackFolder = std::filesystem::path(resourcesPath) / "scores" / "One_track_MIDIs";
    for (auto &p : std::filesystem::recursive_directory_iterator(oneTrackFolder))
    {
        if (p.is_regular_file() && p.path().extension() == ".mid")
        {
            result.push_back(p.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

//------------------------------------------------------------------------
struct TokenizerFactory
{
    static std::shared_ptr<MusicTokenizer> create(const std::string &tokenization, TokenizerConfig &config)
    {
        if (tokenization == "MIDILike")
        {
            return std::make_shared<MIDILike>(config);
        }

        else if (tokenization == "TSD")
        {
            return std::make_shared<TSD>(config);
        }

        else if (tokenization == "REMI")
        {
            return std::make_shared<REMI>(config);
        }

        else if (tokenization == "MMM")
        {
            return std::make_shared<MMM>(config);
        }

        else if (tokenization == "CPWord")
        {
            return std::make_shared<CPWord>(config);
        }

        else if (tokenization == "MuMIDI")
        {
            return std::make_shared<MuMIDI>(config);
        }

        else if (tokenization == "Structured")
        {
            return std::make_shared<Structured>(config);
        }

        else if (tokenization == "Octuple")
        {
            return std::make_shared<Octuple>(config);
        }

        else if (tokenization == "PerTok")
        {
            return std::make_shared<PerTok>(config);
        }

        return nullptr;
    }
};

//------------------------------------------------------------------------
inline std::string midiFileNameFormatter(const std::filesystem::path &path)
{
    std::string name = path.stem().string();
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) { return !std::isalnum(c); }), name.end());
    if (name.empty())
        throw std::runtime_error("unable to format path to name.");
    return name;
}

//------------------------------------------------------------------------
inline std::string midiFileNameFormatter(std::string name)
{
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) { return !std::isalnum(c); }), name.end());
    if (name.empty())
        throw std::runtime_error("unable to format path to name.");
    return name;
}

//------------------------------------------------------------------------
inline TokenizerConfig getBaseTokenizerConfig()
{
    auto config = TokenizerConfig();

    config.specialTokens = {"PAD", "BOS_None", "EOS", "EOS-test_None"};
    config.beatRes       = {
        {  {0, 4}, 8},
        { {4, 12}, 4},
        {{12, 16}, 2}
    };
    config.beatResRest = {
        { {0, 2}, 4},
        {{2, 12}, 2}
    };
    config.numTempos          = 32;
    config.tempoRange         = {40, 250};
    config.timeSignatureRange = {
        {8,            {3, 12, 6}},
        {4, {5, 6, 3, 2, 1, 4, 8}},
        {2,             {2, 3, 4}}
    };
    config.chordMaps                              = chordMaps_default;
    config.chordTokensWithRootNote                = true;
    config.chordUnknown                           = {3, 6};
    config.deleteEqualSuccessiveTempoChanges      = true;
    config.deleteEqualSuccessiveTimeSigChanges    = true;
    config.additionalParams["use_bar_end_tokens"] = true;
    return config;
}

//------------------------------------------------------------------------
inline void adjustTokParamsForTests(std::string tokType, TokenizerConfig &config)
{
    if (tokType == "Structured")
    {
        config.beatRes = {
            {{0, 512}, 8}
        };
    }
    else if (tokType == "Octuple" || tokType == "MuMIDI")
    {
        config.additionalParams["max_bar_embedding"] = maxBarEmbedding_default;
        if (tokType == "Octuple")
        {
            config.useTimeSignatures = false;
        }
    }
    else if (tokType == "CPWord" && config.useTimeSignatures && config.useRests)
    {
        config.useRests = false;
    }
    else if (tokType == "PerTok")
    {
        config.beatRes = {
            {{0, 128}, 4},
            { {0, 32}, 3}
        };
        config.additionalParams["use_microtiming"]       = true;
        config.additionalParams["ticks_per_quarter"]     = 220;
        config.additionalParams["max_microtiming_shift"] = 0.25;
        config.additionalParams["num_microtiming_bins"]  = 110;
        config.additionalParams["use_bar_end_tokens"]    = false;

        config.useRests           = false;
        config.useSustainPedals   = false;
        config.useTempos          = false;
        config.useTimeSignatures  = true;
        config.usePitchBends      = false;
        config.usePitchdrumTokens = false;
        config.usePitchIntervals  = false;
    }
}

//------------------------------------------------------------------------
inline void clipDurations(symusic::pyvec<PedalType> &pedals, std::vector<std::pair<int, int>> maxDurations)
{
    int tpbIdx = 0;
    for (PedalType &pedal : pedals)
    {
        if (pedal.time > maxDurations[tpbIdx].first)
        {
            ++tpbIdx;
        }
        if (pedal.duration > maxDurations[tpbIdx].second)
        {
            pedal.duration = maxDurations[tpbIdx].second;
        }
    }
}

//------------------------------------------------------------------------
inline void clipDurations(symusic::pyvec<NoteType> &notes, std::vector<std::pair<int, int>> maxDurations)
{
    int tpbIdx = 0;
    for (NoteType &note : notes)
    {
        if (note.time > maxDurations[tpbIdx].first)
        {
            ++tpbIdx;
        }
        if (note.duration > maxDurations[tpbIdx].second)
        {
            note.duration = maxDurations[tpbIdx].second;
        }
    }
}

//------------------------------------------------------------------------
inline int timeTokenToTicks(const std::tuple<int, int, int> &tokenDuration, int ticksPerBeat)
{
    int beat = std::get<0>(tokenDuration);
    int pos  = std::get<1>(tokenDuration);
    int res  = std::get<2>(tokenDuration);

    return (beat * res + pos) * ticksPerBeat / res;
}

//------------------------------------------------------------------------
inline void sortScore(ScoreType &score, bool sortTracks = true)
{
    for (symusic::shared<TrackType> track : *score.tracks)
    {
        if (track->is_drum)
        {
            track->program = 0;
        }

        std::stable_sort(track->notes->begin(), track->notes->end(), [](const NoteType &a, const NoteType &b) {
            if (a->time != b->time)
                return a->time < b->time;
            if (a->duration != b->duration)
                return a->duration < b->duration;
            return a->pitch < b->pitch;
        });
    }
    if (sortTracks)
    {
        std::sort(score.tracks->begin(), score.tracks->end(),
                  [](const symusic::shared<TrackType> &a, const symusic::shared<TrackType> &b) {
                      // Sort by program, then by is_drum
                      if (a->program != b->program)
                          return a->program < b->program;
                      return a->is_drum < b->is_drum;
                  });
    }
}

//------------------------------------------------------------------------
// TODO: test this function once Octuple is implemented...
inline void adaptTempoChangesTimes(std::vector<std::shared_ptr<TrackType>>::const_iterator trackBegin,
                                   std::vector<std::shared_ptr<TrackType>>::const_iterator trackEnd,
                                   symusic::shared<symusic::pyvec<TempoType>> tempos, double defaultTempo)
{
    symusic::pyvec<int> times;
    for (auto it = trackBegin; it != trackEnd; ++it)
    {
        auto &track = **it;
        for (const NoteType &note : *track.notes)
        {
            times.append(note.time);
        }
    }
    times.sort();

    if (times.empty() || tempos->empty())
        return;

    // Fix first tempo
    if (std::abs(TempoType::mspq2qpm((*tempos)[0].mspq) - defaultTempo) < 1e-2)
    {
        (*tempos)[0].time = 0;
    }
    else if ((*tempos)[0].time < times[0])
    {
        (*tempos)[0].time = times[0];
    }
    if ((*tempos)[0].time != 0)
    {
        tempos->insert(0, TempoType::from_qpm(0, defaultTempo));
    }

    size_t timeIdx = 0, tempoIdx = 0;
    while (tempoIdx < tempos->size())
    {
        // Delete tempos after last note
        if ((*tempos)[tempoIdx].time > times.back())
        {
            tempos->erase(tempos->begin() + tempoIdx);
            continue;
        }
        // Snap tempos to next note time (except for the first)
        if (tempoIdx > 0)
        {
            for (size_t n = 0; timeIdx + n < times.size(); ++n)
            {
                if (times[timeIdx + n] >= (*tempos)[tempoIdx].time)
                {
                    (*tempos)[tempoIdx].time = times[timeIdx + n];
                    timeIdx += n;
                    break;
                }
            }
        }
        // Delete successive tempos at the same time (keep latest)
        if (tempoIdx > 0 && (*tempos)[tempoIdx].time == (*tempos)[tempoIdx - 1].time)
        {
            tempos->erase(tempos->begin() + tempoIdx - 1);
            --tempoIdx;
        }
        ++tempoIdx;
    }
}

//------------------------------------------------------------------------
inline void adaptRefScoreBeforeTokenize(ScoreType &score, std::shared_ptr<MusicTokenizer> tokenizer)
{
    if (tokenizer->getNoteOnOff())
    {
        if (tokenizer->config.usePrograms && tokenizer->config.oneTokenStreamForPrograms)
        {
            libmusictokUtils::mergeSameProgramTracks(score.tracks);
        }
        // If a max_duration is provided, we clip the durations of the notes before
        // tokenizing, otherwise these notes will be tokenized with durations > to this
        // limit, which would yield errors when checking TSE.
        if (tokenizer->config.additionalParams.find("max_duration") != tokenizer->config.additionalParams.end())
        {
            std::vector<std::pair<int, int>> maxDurations;
            for (std::pair<int, int> tpbPair : libmusictokUtils::getScoreTicksPerBeat(score))
            {
                maxDurations.push_back(
                    {tpbPair.first,
                     timeTokenToTicks(
                         tokenizer->config.additionalParams["max_duration"].get<std::tuple<int, int, int>>(),
                         tpbPair.second)});
            }
            for (symusic::shared<TrackType> &track : *score.tracks)
            {
                clipDurations(*track->notes, maxDurations);
            }
        }

        // This is required for tests, as after resampling, for some overlapping notes
        // with different onset times with the second note ending last (as FIFO), it can
        // happen that after resampling the second note now ends before the first one.
        // Example: POP909_191 at tick 3152 (time division of 16 tpq)
        for (symusic::shared<TrackType> &track : *score.tracks)
        {
            libmusictokUtils::fixOffsetsOverlappingNotes(*track->notes);
        }

        // Now we can sort the notes
        sortScore(score, false);
    }

    // For Octuple, CPWord and MMM, the time signature is carried with the notes.
    // If a Score doesn't have any note, no time signature will be tokenized, and in turn
    // decoded. If that's the case, we simply set time signatures to the default one.
    bool isTypeToFlag = (tokenizer->getType() == TokenizerType::kOctuple ||
                         tokenizer->getType() == TokenizerType::kCPWord || tokenizer->getType() == TokenizerType::kMMM);
    if (tokenizer->config.useTimeSignatures && isTypeToFlag &&
        (score.tracks->size() == 0 || (*score.tracks)[0]->notes->size() == 0))
    {
        std::vector<TimeSigType> defTimeSig = {
            TimeSigType(0, timeSignature_default.first, timeSignature_default.second)};
        score.time_signatures = std::make_shared<symusic::pyvec<TimeSigType>>(defTimeSig);
    }
}

//------------------------------------------------------------------------
inline ScoreType adaptRefScoreForTestsAssertion(const ScoreType &score, std::shared_ptr<MusicTokenizer> tokenizer)
{
    ScoreType scoreOut = tokenizer->preprocessScore(score);

    // For Octuple, as tempo is only carried at notes times, we need to adapt
    // their times for comparison. Set tempo changes at onset times of notes.
    // We use the first track only, as it is the one for which tempos are decoded
    if (tokenizer->config.useTempos && tokenizer->getType() == TokenizerType::kOctuple)
    {
        if (scoreOut.tracks->size() > 0)
        {
            if (tokenizer->config.oneTokenStreamForPrograms)
            {
                adaptTempoChangesTimes(scoreOut.tracks->begin(), scoreOut.tracks->end(), scoreOut.tempos,
                                       tokenizer->getDefaultTempo());
            }
            else
            {
                adaptTempoChangesTimes(scoreOut.tracks->begin(), scoreOut.tracks->begin() + 1, scoreOut.tempos,
                                       tokenizer->getDefaultTempo());
            }
        }
        else
        {
            symusic::pyvec<TempoType> temposVec;
            temposVec.append(TempoType::from_qpm(0, tokenizer->getDefaultTempo()));
            scoreOut.tempos = std::make_shared<symusic::pyvec<TempoType>>(temposVec);
        }
    }

    // If tokenizing each track independently without using Program tokens, the tokenizer
    // will have no way to know the original program of each token sequence when decoding
    // them. We thus resort to set the program of the original score to the default value
    // (0, piano) to match the decoded Score. The content of the tracks (notes, controls)
    // will still be checked.
    if (!tokenizer->config.oneTokenStreamForPrograms && !tokenizer->config.usePrograms)
    {
        for (symusic::shared<TrackType> &track : *scoreOut.tracks)
        {
            track->program = 0;
            track->is_drum = false;
        }
    }

    return scoreOut;
}

//------------------------------------------------------------------------
inline std::string getTokenizerType(std::shared_ptr<MusicTokenizer> tokenizer)
{
    switch (tokenizer->getType())
    {
        case libmusictok::TokenizerType::kBase: throw std::runtime_error("Cannot use Base Tokenizer");

        case libmusictok::TokenizerType::kREMI: return "REMI";

        case libmusictok::TokenizerType::kTSD: return "TSD";

        case libmusictok::TokenizerType::kMIDILike: return "MIDILike";

        case libmusictok::TokenizerType::kMMM: return "MMM";

        case libmusictok::TokenizerType::kStructured: return "Structured";

        case libmusictok::TokenizerType::kOctuple: return "kOctuple";

        case TokenizerType::kMuMIDI: return "MuMIDI";

        case libmusictok::TokenizerType::kCPWord: return "CPWord";

        case libmusictok::TokenizerType::kPerTok: return "PerTok";

        default: throw std::runtime_error("Not a recognized tokenizer");
    }
}

//------------------------------------------------------------------------
inline bool notesAreEqual(NoteType &note1, NoteType &note2, bool checkVelocity = true, bool checkDuration = true)
{
    if (note1.start() != note2.start())
        return false;

    if (checkVelocity && note1.velocity != note2.velocity)
        return false;

    if (checkDuration && note1.end() != note2.end())
        return false;

    if (note1.pitch != note2.pitch)
        return false;

    return true;
}

//------------------------------------------------------------------------
inline std::vector<NoteType> getNotesInRange(int idx, std::shared_ptr<symusic::pyvec<NoteType>> noteList,
                                             int windowSize = 5)
{
    int start = std::max(0, idx - windowSize);
    int end   = std::min(static_cast<int>(noteList->size()) - 1, idx + windowSize);

    std::vector<NoteType> result;
    for (int i = start; i <= end; ++i)
    {
        result.push_back((*noteList)[i]);
    }
    return result;
}

//------------------------------------------------------------------------
inline bool notesInSlidingWindowAreEqual(std::shared_ptr<symusic::pyvec<NoteType>> notes1,
                                         std::shared_ptr<symusic::pyvec<NoteType>> notes2, bool checkVelocities = true,
                                         bool checkDurations = true, int maxTimeRange = 120)
{
    for (size_t idx = 0; idx < notes1->size(); ++idx)
    {
        const auto &note1 = (*notes1)[idx];

        auto potentialNotes = getNotesInRange(static_cast<int>(idx), notes2, 25);

        // filter by pitch
        std::vector<NoteType> filteredNotes;
        for (const auto &note : potentialNotes)
        {
            if (note.pitch == note1.pitch)
                filteredNotes.push_back(note);
        }
        if (filteredNotes.empty())
            return false;

        // check start times
        bool foundStart = false;
        for (const auto &note2 : filteredNotes)
        {
            if (std::abs(note1.start() - note2.start()) < maxTimeRange)
                foundStart = true;
        }
        if (!foundStart)
            return false;

        // check end times
        if (checkDurations)
        {
            bool foundEnd = false;
            for (const auto &note2 : filteredNotes)
            {
                if (std::abs(note1.end() - note2.end()) < maxTimeRange)
                    foundEnd = true;
            }
            if (!foundEnd)
                return false;
        }

        // check velocities
        if (checkVelocities)
        {
            bool foundVelocity = false;
            for (const auto &note2 : filteredNotes)
            {
                if (note1.velocity == note2.velocity)
                    foundVelocity = true;
            }
            if (!foundVelocity)
                return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------
inline bool tracksNotesAreEqual(const std::shared_ptr<TrackType> track1, const std::shared_ptr<TrackType> track2,
                                bool checkVelocities = true, bool checkDurations = true, bool useTimeRange = false,
                                int maxTimeRange = 220)
{
    if (!useTimeRange)
    {
        for (size_t i = 0; i < track1->notes->size(); ++i)
        {
            NoteType &note1 = (*track1->notes)[i];
            NoteType &note2 = (*track2->notes)[i];

            if (!notesAreEqual(note1, note2, checkVelocities, checkDurations))
            {
                return false;
            }
        }
        return true;
    }
    return notesInSlidingWindowAreEqual(track1->notes, track2->notes, checkVelocities, checkDurations, maxTimeRange);
}

//------------------------------------------------------------------------
inline std::vector<std::pair<int, std::string>> scoreNotesAreEqual(const ScoreType &score1, const ScoreType &score2,
                                                                   bool checkVelocities,
                                                                   const std::vector<int> &useNoteDurationPrograms,
                                                                   bool useTimeRange = false)
{
    if (score1.tracks->size() != score2.tracks->size())
    {
        return {
            {0, "num tracks"}
        };
    }

    std::vector<std::pair<int, std::string>> errors;
    for (size_t trackIdx = 0; trackIdx < score1.tracks->size(); ++trackIdx)
    {
        std::shared_ptr<TrackType> track1 = (*score1.tracks)[trackIdx];
        std::shared_ptr<TrackType> track2 = (*score2.tracks)[trackIdx];

        if (track1->program != track2->program || track1->is_drum != track2->is_drum)
        {
            errors.push_back({0, "program"});
        }
        if (track1->notes->size() != track2->notes->size())
        {
            errors.push_back({0, "num notes"});
        }
        int trackProgram        = track1->is_drum ? -1 : track1->program;
        bool usingNoteDurations = std::find(useNoteDurationPrograms.begin(), useNoteDurationPrograms.end(),
                                            trackProgram) != useNoteDurationPrograms.end();

        // Need to set the note durations of the first track to the durations of the
        // second and sort the notes. Without duration tokens, the order of the notes
        // may be altered as during preprocessing they are order by pitch and duration.
        if (!usingNoteDurations)
        {
            for (size_t noteIdx = 0; noteIdx < track1->notes->size(); ++noteIdx)
            {
                (*track1->notes)[noteIdx]->duration = (*track2->notes)[noteIdx]->duration;
            }
        }
        bool trackNotesAreEqual = tracksNotesAreEqual(track1, track2, checkVelocities, usingNoteDurations, useTimeRange,
                                                      score1.ticks_per_quarter * 0.5);
        if (!trackNotesAreEqual)
        {
            errors.push_back({track1->program, "track notes not equal"});
        }
    }
    return errors;
}

//------------------------------------------------------------------------
inline bool temposAreEqual(std::shared_ptr<symusic::pyvec<TempoType>> tempos1,
                           std::shared_ptr<symusic::pyvec<TempoType>> tempos2)
{
    if (tempos1->size() != tempos2->size())
    {
        return false;
    }

    for (size_t tempoIdx = 0; tempoIdx < tempos1->size(); ++tempoIdx)
    {
        const TempoType &tempo1 = (*tempos1)[tempoIdx];
        const TempoType &tempo2 = (*tempos2)[tempoIdx];

        if (tempo1.time != tempo2.time ||
            std::round(tempo1.qpm() * 100) / 100 != std::round(tempo2.qpm() * 100) / 100 ||
            std::fabs(tempo1.mspq - tempo2.mspq) > 1)
        {
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------
inline bool checkScoresAreEqual(const ScoreType &score1, const ScoreType &score2, bool checkVelocities = true,
                                const std::vector<int> &useNoteDurationPrograms = useNoteDurationPrograms_default,
                                bool checkTempos = true, bool checkTimeSignatures = true, bool checkPedals = true,
                                bool checkPitchBends = true, const std::string &logPrefix = std::string(""),
                                bool useTimeRanges = false, int maxTimeRange = 120)
{
    bool hasErrors = false;
    std::vector<std::string> typesOfErrors;

    std::vector<std::pair<int, std::string>> errors =
        scoreNotesAreEqual(score1, score2, checkVelocities, useNoteDurationPrograms, useTimeRanges);

    if (errors.size() > 0)
    {
        hasErrors = true;
        std::clog << logPrefix << " failed to encode/decode NOTES." << std::endl;
    }

    if (checkPedals)
    {
        for (size_t trackIdx = 0; trackIdx < score1.tracks->size(); ++trackIdx)
        {
            const symusic::shared<TrackType> &inst1 = (*score1.tracks)[trackIdx];
            const symusic::shared<TrackType> &inst2 = (*score2.tracks)[trackIdx];

            if (!useTimeRanges && *inst1->pedals != *inst2->pedals)
            {
                typesOfErrors.push_back("PEDALS");
                break;
            }

            for (size_t pedalIdx = 0; pedalIdx < inst1->pedals->size(); ++pedalIdx)
            {
                const PedalType &pedal1 = (*inst1->pedals)[pedalIdx];
                const PedalType &pedal2 = (*inst2->pedals)[pedalIdx];

                if ((pedal1.time - pedal2.time) > maxTimeRange || (pedal1.duration - pedal2.duration) > maxTimeRange)
                {
                    typesOfErrors.push_back("PEDALS");
                    break;
                }
            }
        }
    }

    if (checkPitchBends)
    {
        for (size_t trackIdx = 0; trackIdx < score1.tracks->size(); ++trackIdx)
        {
            const symusic::shared<TrackType> &inst1 = (*score1.tracks)[trackIdx];
            const symusic::shared<TrackType> &inst2 = (*score2.tracks)[trackIdx];

            if (*inst1->pitch_bends != *inst2->pitch_bends)
            {
                typesOfErrors.push_back("PITCH BENDS");
            }
        }
    }

    if (checkTempos && !temposAreEqual(score1.tempos, score2.tempos))
    {
        typesOfErrors.push_back("TEMPOS");
    }

    if (checkTimeSignatures)
    {
        if (!useTimeRanges && *score1.time_signatures != *score2.time_signatures)
        {
            typesOfErrors.push_back("TIME SIGNATURES");
        }
        else if (useTimeRanges)
        {
            for (size_t tsIdx = 0; tsIdx < score1.time_signatures->size(); ++tsIdx)
            {
                const TimeSigType &timeSig1 = (*score1.time_signatures)[tsIdx];
                const TimeSigType &timeSig2 = (*score2.time_signatures)[tsIdx];

                if (std::fabs(timeSig1.time - timeSig2.time) > maxTimeRange)
                {
                    typesOfErrors.push_back("TIME SIGNATURES");
                    break;
                }
            }
        }
    }

    hasErrors = hasErrors || (typesOfErrors.size() > 0);
    for (const std::string errType : typesOfErrors)
    {
        std::clog << logPrefix << " failed to encode/decode " << errType << std::endl;
    }
    return !hasErrors;
}

//------------------------------------------------------------------------
inline std::tuple<ScoreType, ScoreType, bool> tokenizeAndCheckEquals(ScoreType score,
                                                                     std::shared_ptr<MusicTokenizer> tokenizer,
                                                                     std::filesystem::path filename)
{
    const std::string tokenization = getTokenizerType(tokenizer);

    const std::string logPrefix = filename.filename().string() + " / " + tokenization;

    bool useTimeRanges = tokenizer->getType() == TokenizerType::kPerTok;

    // Tokenize and detokenize
    adaptRefScoreBeforeTokenize(score, tokenizer);
    auto tokens = tokenizer->encode(score);
    ScoreType scoreDecoded;

    if (std::holds_alternative<TokSequence>(tokens))
    {
        scoreDecoded = tokenizer->decode(std::get<TokSequence>(tokens));
    }
    if (std::holds_alternative<TokSequenceVec>(tokens))
    {
        scoreDecoded = tokenizer->decode(std::get<TokSequenceVec>(tokens));
    }

    // Postprocess the reference and decoded scores
    // We might need to resample the original preprocessed Score, as it has been
    // resampled with its highest ticks/beat whereas the tokens has been decoded with
    // the tokenizer's time division, which can be different if using time signatures.
    score = adaptRefScoreForTestsAssertion(score, tokenizer);
    if (score.ticks_per_quarter != scoreDecoded.ticks_per_quarter)
    {
        score = symusic::resample(score, scoreDecoded.ticks_per_quarter, 0);
    }

    sortScore(score);
    sortScore(scoreDecoded);

    // Check decoded score is identical to the reference one
    bool scoresAreEqual =
        checkScoresAreEqual(score, scoreDecoded, tokenizer->config.useVelocities,
                            std::vector<int>(tokenizer->config.useNoteDurationPrograms.begin(),
                                             tokenizer->config.useNoteDurationPrograms.end()),
                            tokenizer->config.useTempos && tokenizer->getType() != TokenizerType::kMuMIDI,
                            tokenizer->config.useTimeSignatures, tokenizer->config.useSustainPedals,
                            tokenizer->config.usePitchBends, logPrefix, useTimeRanges);

    std::variant<float, std::vector<float>> errTse;

    if (std::holds_alternative<TokSequence>(tokens))
    {
        errTse = tokenizer->tokensErrors(std::get<TokSequence>(tokens));
    }
    if (std::holds_alternative<TokSequenceVec>(tokens))
    {
        errTse = tokenizer->tokensErrors(std::get<TokSequenceVec>(tokens));
    }

    double errTseD;

    if (std::holds_alternative<std::vector<float>>(errTse))
    {
        errTseD = std::accumulate(std::get<std::vector<float>>(errTse).begin(),
                                  std::get<std::vector<float>>(errTse).end(), 0.0);
    }
    else
    {
        errTseD = std::get<float>(errTse);
    }

    if (errTseD != 0.0)
    {
        scoresAreEqual = false;
        std::clog << logPrefix << " Validation of tokens types / values successions failed" << std::endl;
    }
    return std::make_tuple(scoreDecoded, score, !scoresAreEqual);
}

//------------------------------------------------------------------------
}; // namespace libmusictokTest
