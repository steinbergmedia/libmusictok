//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/tok_sequence.cpp
// Created by  : Steinberg, 02/2025
// Description : TokSequence class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tok_sequence.h"
#include <algorithm>
#include <stdexcept>

namespace libmusictok {

//------------------------------------------------------------------------
// Constructor
TokSequence::TokSequence(std::vector<std::string> tokens, std::vector<int> ids, std::u32string bytes,
                         std::vector<Event> events, bool areIdsEncoded)
    : tokens{tokens}
    , ids{ids}
    , bytes{bytes}
    , events{events}
    , areIdsEncoded{areIdsEncoded}
{
}

//------------------------------------------------------------------------
// Public methods
//------------------------------------------------------------------------

//------------------------------------------------------------------------
std::vector<TokSequence> TokSequence::splitPerBars() const
{
    return splitPerTicks(ticksBars);
}

//------------------------------------------------------------------------
std::vector<TokSequence> TokSequence::splitPerBeats() const
{
    return splitPerTicks(ticksBeats);
}

//------------------------------------------------------------------------
size_t TokSequence::size() const
{
    if (!ids.empty() && areIdsEncoded)
        return ids.size();

    if (!tokens.empty())
        return tokens.size();

    if (!events.empty())
        return events.size();

    if (!bytes.empty())
        return bytes.size();

    return 0;
}

//------------------------------------------------------------------------
bool TokSequence::operator==(const TokSequence &other) const
{
    bool oneCommonAttribute = false;

    if (!ids.empty() && !other.ids.empty())
    {
        oneCommonAttribute = true;
        if (ids != other.ids)
            return false;
    }

    if (!tokens.empty() && !other.tokens.empty())
    {
        oneCommonAttribute = true;
        if (tokens != other.tokens)
            return false;
    }

    if (!bytes.empty() && !other.bytes.empty())
    {
        oneCommonAttribute = true;
        if (bytes != other.bytes)
            return false;
    }

    if (!events.empty() && !other.events.empty())
    {
        oneCommonAttribute = true;
        if (events != other.events)
            return false;
    }

    return oneCommonAttribute;
}

//------------------------------------------------------------------------
TokSequence TokSequence::operator+(const TokSequence &other) const
{
    TokSequence newSeq = *this;
    newSeq += other;
    return newSeq;
}

//------------------------------------------------------------------------
TokSequence &TokSequence::operator+=(const TokSequence &other)
{
    if (!other.tokens.empty())
        tokens.insert(tokens.end(), other.tokens.begin(), other.tokens.end());
    if (!other.ids.empty())
        ids.insert(ids.end(), other.ids.begin(), other.ids.end());
    if (!other.bytes.empty())
        bytes += other.bytes;
    if (!other.events.empty())
        events.insert(events.end(), other.events.begin(), other.events.end());

    return *this;
}

//------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------

std::vector<TokSequence> TokSequence::splitPerTicks(const std::vector<int> &ticks) const
{
    std::vector<size_t> idxs = {0};
    size_t ti_prev           = 0;
    for (size_t bi = 1; bi < ticks.size(); ++bi)
    {
        size_t ti = ti_prev;
        while (ti < events.size() && events.get()[ti].time < ticks[bi])
        {
            ++ti;
        }
        if (ti == events.size())
        {
            break;
        }
        idxs.push_back(ti);
        ti_prev = ti;
    }

    idxs.push_back(events.size());
    std::vector<TokSequence> subsequences;
    for (size_t i = 1; i < idxs.size(); ++i)
    {
        TokSequence subseq = this->slice(idxs[i - 1], idxs[i]);
        subseq.ticksBars.clear();
        subseq.ticksBeats.clear();
        subsequences.push_back(subseq);
    }
    TokSequence finalSubseq = this->sliceUntilEnd(idxs[idxs.size() - 1]);
    finalSubseq.ticksBars.clear();
    finalSubseq.ticksBeats.clear();
    subsequences.push_back(finalSubseq);

    return subsequences;
}

//------------------------------------------------------------------------
TokSequence TokSequence::slice(size_t start, size_t end) const
{
    TokSequence seq = *this;
    if (!tokens.empty())
        seq.tokens = std::vector<std::string>(tokens.begin() + start, tokens.begin() + end);
    if (!ids.empty())
        seq.ids = std::vector<int>(ids.begin() + start, ids.begin() + end);
    if (!bytes.empty())
        seq.bytes = bytes.substr(start, end);
    if (!events.empty())
        seq.events = std::vector<Event>(events.begin() + start, events.begin() + end);
    return seq;
}

//------------------------------------------------------------------------
TokSequence TokSequence::sliceUntilEnd(size_t start) const
{
    TokSequence seq = *this;
    if (!tokens.empty())
        seq.tokens = std::vector<std::string>(tokens.begin() + start, tokens.end());

    if (!ids.empty())
        seq.ids = std::vector<int>(ids.begin() + start, ids.end());

    if (!bytes.empty())
        seq.bytes = bytes.substr(start, bytes.size());

    if (!events.empty())
        seq.events = std::vector<Event>(events.begin() + start, events.end());

    return seq;
}

//------------------------------------------------------------------------
void TokSequence::setTicksBars(std::vector<int> ticksBars_)
{
    this->ticksBars = ticksBars_;
}

//------------------------------------------------------------------------
void TokSequence::setTicksBeats(std::vector<int> ticksBeats_)
{
    this->ticksBeats = ticksBeats_;
}

//------------------------------------------------------------------------
} // namespace libmusictok
