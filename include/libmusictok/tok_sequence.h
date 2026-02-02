//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/tok_sequence.h
// Created by  : Steinberg, 02/2025
// Description : TokSequence class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include "libmusictok/event.h"

#include <cassert>
#include <codecvt>
#include <locale>
#include <string>
#include <vector>

namespace libmusictok {

// Definitions for a Tokens, Bytes, Ids, and Events containers.
// TokSequence Definition below.
namespace TokSeqAPI {

//------------------------------------------------------------------------
class Tokens
{
public:
    Tokens(std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> tokens =
               std::vector<std::string>())
        : tokensVariant(tokens)
    {
    }

    size_t size() const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<std::string>>>(tokensVariant).size();

        else
            return std::get<std::vector<std::string>>(tokensVariant).size();
    }

    bool isMultiVoc() const
    {
        return std::holds_alternative<std::vector<std::vector<std::string>>>(tokensVariant);
    }

    //------------------------------------------------------------------------
    const std::vector<std::string> &get() const
    {
        if (isMultiVoc())
            throw std::runtime_error("This Tokens object is multivoc. Use getMultiVoc() instead of get()");

        return std::get<std::vector<std::string>>(tokensVariant);
    }

    const std::vector<std::vector<std::string>> &getMultiVoc() const
    {
        if (!isMultiVoc())
            throw std::runtime_error("This Tokens object is not multivoc. Use get() instead of getMultiVoc()");

        return std::get<std::vector<std::vector<std::string>>>(tokensVariant);
    }

    //------------------------------------------------------------------------
    Tokens &operator=(const std::vector<std::string> &other)
    {
        tokensVariant = other;
        return *this;
    }

    Tokens &operator=(const std::vector<std::vector<std::string>> &other)
    {
        tokensVariant = other;
        return *this;
    }

    //------------------------------------------------------------------------
    std::variant<std::vector<std::string>, std::string> operator[](size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<std::string>>>(tokensVariant)[index];

        else
            return std::get<std::vector<std::string>>(tokensVariant)[index];
    }

    std::variant<std::vector<std::string>, std::string> at(size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<std::string>>>(tokensVariant).at(index);

        else
            return std::get<std::vector<std::string>>(tokensVariant).at(index);
    }

    //------------------------------------------------------------------------
    bool empty() const
    {
        return size() == 0;
    }

    void clear()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<std::string>>>(tokensVariant).clear();

        else
            std::get<std::vector<std::string>>(tokensVariant).clear();
    }

    //------------------------------------------------------------------------
    void pushBack(const std::variant<std::vector<std::string>, std::string> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<std::string>>(item))
            std::get<std::vector<std::vector<std::string>>>(tokensVariant)
                .push_back(std::get<std::vector<std::string>>(item));

        else if (!isMultiVoc() && std::holds_alternative<std::string>(item))
            std::get<std::vector<std::string>>(tokensVariant).push_back(std::get<std::string>(item));
    }

    void popBack()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<std::string>>>(tokensVariant).pop_back();

        else
            std::get<std::vector<std::string>>(tokensVariant).pop_back();
    }

    void insert(size_t position, const std::variant<std::vector<std::string>, std::string> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<std::string>>(item))
        {
            auto &vec = std::get<std::vector<std::vector<std::string>>>(tokensVariant);
            vec.insert(vec.begin() + position, std::get<std::vector<std::string>>(item));
        }
        else if (!isMultiVoc() && std::holds_alternative<std::string>(item))
        {
            auto &vec = std::get<std::vector<std::string>>(tokensVariant);
            vec.insert(vec.begin() + position, std::get<std::string>(item));
        }
        else
        {
            std::runtime_error("item being inserted doesn't match the type of the array being inserted to");
        }
    }

    void insert(std::vector<std::string>::const_iterator position, std::vector<std::string>::const_iterator first,
                std::vector<std::string>::const_iterator last)
    {
        if (isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<std::string>>(tokensVariant);
        vec.insert(position, first, last);
    }

    void insert(std::vector<std::vector<std::string>>::const_iterator position,
                std::vector<std::vector<std::string>>::const_iterator first,
                std::vector<std::vector<std::string>>::const_iterator last)
    {
        if (!isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<std::vector<std::string>>>(tokensVariant);
        vec.insert(position, first, last);
    }

    //------------------------------------------------------------------------
    bool operator==(const Tokens &other) const
    {
        return tokensVariant == other.tokensVariant;
    }

    bool operator!=(const Tokens &other) const
    {
        return !(*this == other);
    }

    //------------------------------------------------------------------------
    std::vector<std::string>::const_iterator begin() const
    {
        if (isMultiVoc())
            std::runtime_error("call beginMultiVoc() rather than begin()");

        return std::get<std::vector<std::string>>(tokensVariant).begin();
    }

    std::vector<std::string>::const_iterator end() const
    {
        if (isMultiVoc())
            std::runtime_error("call endMultiVoc() rather than end()");

        return std::get<std::vector<std::string>>(tokensVariant).end();
    }

    std::vector<std::vector<std::string>>::const_iterator beginMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call begin() rather than beginMultiVoc()");

        return std::get<std::vector<std::vector<std::string>>>(tokensVariant).begin();
    }

    std::vector<std::vector<std::string>>::const_iterator endMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call end() rather than endMultiVoc()");

        return std::get<std::vector<std::vector<std::string>>>(tokensVariant).end();
    }

private:
    std::variant<std::vector<std::vector<std::string>>, std::vector<std::string>> tokensVariant;
};

//------------------------------------------------------------------------
class Ids
{
public:
    Ids(std::variant<std::vector<std::vector<int>>, std::vector<int>> ids = std::vector<int>())
        : idsVariant(ids)
    {
    }

    size_t size() const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<int>>>(idsVariant).size();

        else
            return std::get<std::vector<int>>(idsVariant).size();
    }

    bool isMultiVoc() const
    {
        return std::holds_alternative<std::vector<std::vector<int>>>(idsVariant);
    }

    //------------------------------------------------------------------------
    const std::vector<int> &get() const
    {
        if (isMultiVoc())
            throw std::runtime_error("This Ids object is multivoc. Use getMultiVoc() instead of get()");

        return std::get<std::vector<int>>(idsVariant);
    }

    const std::vector<std::vector<int>> &getMultiVoc() const
    {
        if (!isMultiVoc())
            throw std::runtime_error("This Ids object is not multivoc. Use get() instead of getMultiVoc()");

        return std::get<std::vector<std::vector<int>>>(idsVariant);
    }

    //------------------------------------------------------------------------
    Ids &operator=(const std::vector<int> &other)
    {
        idsVariant = other;
        return *this;
    }

    Ids &operator=(const std::vector<std::vector<int>> &other)
    {
        idsVariant = other;
        return *this;
    }

    //------------------------------------------------------------------------
    std::variant<std::vector<int>, int> operator[](size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<int>>>(idsVariant)[index];

        else
            return std::get<std::vector<int>>(idsVariant)[index];
    }

    std::variant<std::vector<int>, int> at(size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<int>>>(idsVariant).at(index);

        else
            return std::get<std::vector<int>>(idsVariant).at(index);
    }

    //------------------------------------------------------------------------
    bool empty() const
    {
        return size() == 0;
    }

    void clear()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<int>>>(idsVariant).clear();

        else
            std::get<std::vector<int>>(idsVariant).clear();
    }

    //------------------------------------------------------------------------
    void pushBack(const std::variant<std::vector<int>, int> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<int>>(item))
            std::get<std::vector<std::vector<int>>>(idsVariant).push_back(std::get<std::vector<int>>(item));

        else if (!isMultiVoc() && std::holds_alternative<int>(item))
            std::get<std::vector<int>>(idsVariant).push_back(std::get<int>(item));
    }

    void popBack()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<int>>>(idsVariant).pop_back();

        else
            std::get<std::vector<int>>(idsVariant).pop_back();
    }

    void insert(size_t position, const std::variant<std::vector<int>, int> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<int>>(item))
        {
            auto &vec = std::get<std::vector<std::vector<int>>>(idsVariant);
            vec.insert(vec.begin() + position, std::get<std::vector<int>>(item));
        }
        else if (!isMultiVoc() && std::holds_alternative<int>(item))
        {
            auto &vec = std::get<std::vector<int>>(idsVariant);
            vec.insert(vec.begin() + position, std::get<int>(item));
        }
        else
        {
            std::runtime_error("item being inserted doesn't match the type of the array being inserted to");
        }
    }

    void insert(std::vector<int>::const_iterator position, std::vector<int>::const_iterator first,
                std::vector<int>::const_iterator last)
    {
        if (isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<int>>(idsVariant);
        vec.insert(position, first, last);
    }

    void insert(std::vector<std::vector<int>>::const_iterator position,
                std::vector<std::vector<int>>::const_iterator first, std::vector<std::vector<int>>::const_iterator last)
    {
        if (!isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<std::vector<int>>>(idsVariant);
        vec.insert(position, first, last);
    }

    //------------------------------------------------------------------------
    bool operator==(const Ids &other) const
    {
        return idsVariant == other.idsVariant;
    }

    bool operator!=(const Ids &other) const
    {
        return !(*this == other);
    }

    //------------------------------------------------------------------------
    std::vector<int>::const_iterator begin() const
    {
        if (isMultiVoc())
            std::runtime_error("call beginMultiVoc() rather than begin()");

        return std::get<std::vector<int>>(idsVariant).begin();
    }

    std::vector<int>::const_iterator end() const
    {
        if (isMultiVoc())
            std::runtime_error("call endMultiVoc() rather than end()");

        return std::get<std::vector<int>>(idsVariant).end();
    }

    std::vector<std::vector<int>>::const_iterator beginMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call begin() rather than beginMultiVoc()");

        return std::get<std::vector<std::vector<int>>>(idsVariant).begin();
    }

    std::vector<std::vector<int>>::const_iterator endMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call end() rather than endMultiVoc()");

        return std::get<std::vector<std::vector<int>>>(idsVariant).end();
    }

private:
    std::variant<std::vector<std::vector<int>>, std::vector<int>> idsVariant;
};

//------------------------------------------------------------------------
class Bytes
{
public:
    Bytes(std::variant<std::vector<std::u32string>, std::u32string> tokens = std::vector<std::u32string>())
        : bytesVariant(tokens)
    {
    }

    size_t size() const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::u32string>>(bytesVariant).size();

        else
            return std::get<std::u32string>(bytesVariant).size();
    }

    bool isMultiVoc() const
    {
        return std::holds_alternative<std::vector<std::u32string>>(bytesVariant);
    }

    //------------------------------------------------------------------------
    const std::u32string &get() const
    {
        if (isMultiVoc())
            throw std::runtime_error("This Bytes object is multivoc. Use getMultiVoc() instead of get()");

        return std::get<std::u32string>(bytesVariant);
    }

    const std::vector<std::u32string> &getMultiVoc() const
    {
        if (!isMultiVoc())
            throw std::runtime_error("This Bytes object is not multivoc. Use get() instead of getMultiVoc()");

        return std::get<std::vector<std::u32string>>(bytesVariant);
    }

    //------------------------------------------------------------------------
    Bytes &operator=(const std::u32string &other)
    {
        bytesVariant = other;
        return *this;
    }

    Bytes &operator=(const std::vector<std::u32string> &other)
    {
        bytesVariant = other;
        return *this;
    }

    //------------------------------------------------------------------------
    Bytes &operator+=(const std::vector<std::u32string> &other)
    {
        if (!isMultiVoc())
            throw std::runtime_error("Cannot += std::vector<std::u32string> to this Bytes object");

        insert(this->end(), other.begin(), other.end());
        return *this;
    }

    Bytes &operator+=(const std::u32string &other)
    {
        if (isMultiVoc())
            throw std::runtime_error("Cannot += std::u32string to this Bytes object");

        bytesVariant = std::get<std::u32string>(bytesVariant) += other;
        return *this;
    }

    Bytes &operator+=(const Bytes &other)
    {
        if (isMultiVoc())
            insert(this->end(), other.begin(), other.end());

        else
            bytesVariant = std::get<std::u32string>(bytesVariant) += std::get<std::u32string>(other.bytesVariant);

        return *this;
    }

    Bytes substr(size_t startPos, size_t endPos) const
    {
        assert(endPos >= startPos);
        if (isMultiVoc())
            return Bytes(std::vector<std::u32string>(this->begin() + startPos, this->begin() + endPos));

        else
            return Bytes(std::get<std::u32string>(bytesVariant).substr(startPos, endPos - startPos));
    }

    //------------------------------------------------------------------------
    bool empty() const
    {
        return size() == 0;
    }

    void clear()
    {
        if (isMultiVoc())
            std::get<std::vector<std::u32string>>(bytesVariant).clear();

        else
            std::get<std::u32string>(bytesVariant).clear();
    }

    //------------------------------------------------------------------------
    void pushBack(const std::u32string &item)
    {
        if (!isMultiVoc())
            throw std::runtime_error("Cannot pushBack");

        std::get<std::vector<std::u32string>>(bytesVariant).push_back(item);
    }

    void popBack()
    {
        if (!isMultiVoc())
            throw std::runtime_error("Cannot popBack");

        std::get<std::vector<std::u32string>>(bytesVariant).pop_back();
    }

    void insert(size_t position, const std::u32string &item)
    {
        if (!isMultiVoc())
            std::runtime_error("Cannot insert if is not multivoc");

        auto &vec = std::get<std::vector<std::u32string>>(bytesVariant);
        vec.insert(vec.begin() + position, item);
    }

    void insert(std::vector<std::u32string>::const_iterator position, std::vector<std::u32string>::const_iterator first,
                std::vector<std::u32string>::const_iterator last)
    {
        if (isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<std::u32string>>(bytesVariant);
        vec.insert(position, first, last);
    }

    //------------------------------------------------------------------------
    bool operator==(const Bytes &other) const
    {
        return bytesVariant == other.bytesVariant;
    }

    bool operator!=(const Bytes &other) const
    {
        return !(*this == other);
    }

    //------------------------------------------------------------------------
    std::vector<std::u32string>::const_iterator begin() const
    {
        if (isMultiVoc())
            std::runtime_error("call beginMultiVoc() rather than begin()");

        return std::get<std::vector<std::u32string>>(bytesVariant).begin();
    }

    std::vector<std::u32string>::const_iterator end() const
    {
        if (isMultiVoc())
            std::runtime_error("call endMultiVoc() rather than end()");

        return std::get<std::vector<std::u32string>>(bytesVariant).end();
    }

private:
    std::variant<std::vector<std::u32string>, std::u32string> bytesVariant;
};

//------------------------------------------------------------------------
class Events
{
public:
    Events(std::variant<std::vector<std::vector<Event>>, std::vector<Event>> ids = std::vector<Event>())
        : eventsVariant(ids)
    {
    }

    size_t size() const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<Event>>>(eventsVariant).size();

        else
            return std::get<std::vector<Event>>(eventsVariant).size();
    }

    bool isMultiVoc() const
    {
        return std::holds_alternative<std::vector<std::vector<Event>>>(eventsVariant);
    }

    //------------------------------------------------------------------------
    const std::vector<Event> &get() const
    {
        if (isMultiVoc())
            throw std::runtime_error("This Events object is multivoc. Use getMultiVoc() instead of get()");

        return std::get<std::vector<Event>>(eventsVariant);
    }

    const std::vector<std::vector<Event>> &getMultiVoc() const
    {
        if (!isMultiVoc())
            throw std::runtime_error("This Events object is not multivoc. Use get() instead of getMultiVoc()");

        return std::get<std::vector<std::vector<Event>>>(eventsVariant);
    }

    //------------------------------------------------------------------------
    Events &operator=(const std::vector<Event> &other)
    {
        eventsVariant = other;
        return *this;
    }

    Events &operator=(const std::vector<std::vector<Event>> &other)
    {
        eventsVariant = other;
        return *this;
    }

    //------------------------------------------------------------------------
    std::variant<std::vector<Event>, Event> operator[](size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<Event>>>(eventsVariant)[index];

        else
            return std::get<std::vector<Event>>(eventsVariant)[index];
    }

    std::variant<std::vector<Event>, Event> at(size_t index) const
    {
        if (isMultiVoc())
            return std::get<std::vector<std::vector<Event>>>(eventsVariant).at(index);

        else
            return std::get<std::vector<Event>>(eventsVariant).at(index);
    }

    //------------------------------------------------------------------------
    bool empty() const
    {
        return size() == 0;
    }

    void clear()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<Event>>>(eventsVariant).clear();

        else
            std::get<std::vector<Event>>(eventsVariant).clear();
    }

    //------------------------------------------------------------------------
    void pushBack(const std::variant<std::vector<Event>, Event> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<Event>>(item))
            std::get<std::vector<std::vector<Event>>>(eventsVariant).push_back(std::get<std::vector<Event>>(item));

        else if (!isMultiVoc() && std::holds_alternative<Event>(item))
            std::get<std::vector<Event>>(eventsVariant).push_back(std::get<Event>(item));
    }

    void popBack()
    {
        if (isMultiVoc())
            std::get<std::vector<std::vector<Event>>>(eventsVariant).pop_back();

        else
            std::get<std::vector<Event>>(eventsVariant).pop_back();
    }

    void insert(size_t position, const std::variant<std::vector<Event>, Event> &item)
    {
        if (isMultiVoc() && std::holds_alternative<std::vector<Event>>(item))
        {
            auto &vec = std::get<std::vector<std::vector<Event>>>(eventsVariant);
            vec.insert(vec.begin() + position, std::get<std::vector<Event>>(item));
        }
        else if (!isMultiVoc() && std::holds_alternative<Event>(item))
        {
            auto &vec = std::get<std::vector<Event>>(eventsVariant);
            vec.insert(vec.begin() + position, std::get<Event>(item));
        }
        else
        {
            std::runtime_error("item being inserted doesn't match the type of the array being inserted to");
        }
    }

    void insert(std::vector<Event>::const_iterator position, std::vector<Event>::const_iterator first,
                std::vector<Event>::const_iterator last)
    {
        if (isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<Event>>(eventsVariant);
        vec.insert(position, first, last);
    }

    void insert(std::vector<std::vector<Event>>::const_iterator position,
                std::vector<std::vector<Event>>::const_iterator first,
                std::vector<std::vector<Event>>::const_iterator last)
    {
        if (!isMultiVoc())
            std::runtime_error("");

        auto &vec = std::get<std::vector<std::vector<Event>>>(eventsVariant);
        vec.insert(position, first, last);
    }

    //------------------------------------------------------------------------
    bool operator==(const Events &other) const
    {
        return eventsVariant == other.eventsVariant;
    }

    bool operator!=(const Events &other) const
    {
        return !(*this == other);
    }

    //------------------------------------------------------------------------
    std::vector<Event>::const_iterator begin() const
    {
        if (isMultiVoc())
            std::runtime_error("call beginMultiVoc() rather than begin()");

        return std::get<std::vector<Event>>(eventsVariant).begin();
    }

    std::vector<Event>::const_iterator end() const
    {
        if (isMultiVoc())
            std::runtime_error("call endMultiVoc() rather than end()");

        return std::get<std::vector<Event>>(eventsVariant).end();
    }

    std::vector<std::vector<Event>>::const_iterator beginMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call begin() rather than beginMultiVoc()");

        return std::get<std::vector<std::vector<Event>>>(eventsVariant).begin();
    }

    std::vector<std::vector<Event>>::const_iterator endMultiVoc() const
    {
        if (!isMultiVoc())
            std::runtime_error("call end() rather than endMultiVoc()");

        return std::get<std::vector<std::vector<Event>>>(eventsVariant).end();
    }

private:
    std::variant<std::vector<std::vector<Event>>, std::vector<Event>> eventsVariant;
};

}; // namespace TokSeqAPI

//------------------------------------------------------------------------
class TokSequence
{
public:
    TokSequence(std::vector<std::string> tokens = {}, std::vector<int> ids = {}, std::u32string bytes = {},
                std::vector<Event> events = {}, bool areIdsEncoded = false);

    ///< Splits the TokSequence into a vector of TokSequences, each of a Bar
    std::vector<TokSequence> splitPerBars() const;

    ///< Splits the TokSequence into a vector of TokSequences, each of a Beat
    std::vector<TokSequence> splitPerBeats() const;

    ///< Returns the size of the TokSequence.
    size_t size() const;

    ///< Returns a portion of the TokSequence
    TokSequence slice(size_t start, size_t end) const;

    ///< Returns an portion of the TokSequence
    TokSequence sliceUntilEnd(size_t start) const;

    ///< Sets the ticks for the bar onsets
    void setTicksBars(std::vector<int> ticksBars_);

    ///< Sets the ticks for the beat onsets
    void setTicksBeats(std::vector<int> ticksBeats_);

    bool operator==(const TokSequence &other) const;
    TokSequence operator+(const TokSequence &other) const;
    TokSequence &operator+=(const TokSequence &other);

    //------------------------------------------------------------------------
    // Vars
    TokSeqAPI::Tokens tokens;
    TokSeqAPI::Ids ids;
    TokSeqAPI::Bytes bytes;
    TokSeqAPI::Events events;
    bool areIdsEncoded = false;

private:
    std::vector<TokSequence> splitPerTicks(const std::vector<int> &ticks) const;

    //------------------------------------------------------------------------
    std::vector<int> ticksBars;
    std::vector<int> ticksBeats;
};
} // namespace libmusictok
