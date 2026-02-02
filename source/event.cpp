//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Helpers
// Filename    : source/event.cpp
// Created by  : Steinberg, 02/2025
// Description : Event class implementation
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/event.h"

#include <iostream>

namespace libmusictok {

//------------------------------------------------------------------------
Event::Event(const std::string &type_, const std::variant<std::string, int> &value_, int time_, int program_,
             std::variant<std::string, int> desc_)
    : time(time_)
    , type(type_)
    , value(value_)
    , program(program_)
    , desc(desc_)
{
}

//------------------------------------------------------------------------
std::string Event::str() const
{
    return type + "_" +
           std::visit(
               [](auto &&arg) -> std::string {
                   if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, int>)
                       return std::to_string(arg);
                   else
                       return arg;
               },
               value);
}

//------------------------------------------------------------------------
std::string Event::repr() const
{
    return "Event(type=" + type + ", value=" +
           std::visit(
               [](auto &&arg) -> std::string {
                   if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, int>)
                       return std::to_string(arg);
                   else
                       return arg;
               },
               value) +
           ", time=" + std::to_string(time) + ", desc=" +
           std::visit(
               [](auto &&arg) -> std::string {
                   if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, int>)
                       return std::to_string(arg);
                   else
                       return arg;
               },
               desc) +
           ")";
}

//------------------------------------------------------------------------
bool Event::operator==(const Event &other) const
{
    return (type == other.type && value == other.value && program == other.program && desc == other.desc);
}

} // namespace libmusictok
