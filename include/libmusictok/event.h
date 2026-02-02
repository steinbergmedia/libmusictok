//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : include/libmusictok/event.h
// Created by  : Steinberg, 02/2025
// Description : Event class definition
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#pragma once

#include <string>
#include <variant>

//------------------------------------------------------------------------
namespace libmusictok {

//------------------------------------------------------------------------
class Event
{
public:
    // Methods
    Event(const std::string &type_, const std::variant<std::string, int> &value_, int time_ = -1, int program_ = 0,
          std::variant<std::string, int> desc_ = 0);

    ///< Like Python's __str__()
    std::string str() const;

    ///< Like Python's __repr__()
    std::string repr() const;

    bool operator==(const Event &other) const;

    // Vars
    int time;
    std::string type;
    std::variant<std::string, int> value;
    int program;
    std::variant<std::string, int> desc;
};

} // namespace libmusictok
