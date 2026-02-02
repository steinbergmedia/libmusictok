//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/event_test.cpp
// Created by  : Steinberg, 03/2025
// Description : Tests the Event class
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include <gtest/gtest.h>

//------------------------------------------------------------------------

#include "libmusictok/event.h"

//------------------------------------------------------------------------
using namespace libmusictok;
namespace libmusictokTest {
//------------------------------------------------------------------------
class EventTest : public ::testing::Test
{
protected:
    EventTest() {}

    virtual void SetUp() {}
};

//------------------------------------------------------------------------
TEST_F(EventTest, strTest)
{
    Event event1 = Event("note-on", 3);
    Event event2 = Event("note-off", "4-ticks");

    std::string str1 = "note-on_3";
    std::string str2 = "note-off_4-ticks";

    ASSERT_EQ(str1, event1.str());
    ASSERT_EQ(str2, event2.str());
}

//------------------------------------------------------------------------
TEST_F(EventTest, reprTest)
{
    Event event1 = Event("note-on", 3);
    Event event2 = Event("note-off", "4-ticks", 2, 1, "This event is a note off");

    std::string repr1 = "Event(type=note-on, value=3, time=-1, desc=0)";
    std::string repr2 = "Event(type=note-off, value=4-ticks, time=2, desc=This event is a note off)";

    ASSERT_EQ(repr1, event1.repr());
    ASSERT_EQ(repr2, event2.repr());
}

//------------------------------------------------------------------------
} // namespace libmusictokTest
