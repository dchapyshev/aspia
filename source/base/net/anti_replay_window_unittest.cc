//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/net/anti_replay_window.h"

#include <gtest/gtest.h>

namespace base {

TEST(AntiReplayWindowTest, ZeroCounterRejected)
{
    AntiReplayWindow window;
    EXPECT_FALSE(window.check(0));
}

TEST(AntiReplayWindowTest, SequentialAccepted)
{
    AntiReplayWindow window;
    for (quint64 i = 1; i <= 100; ++i)
    {
        EXPECT_TRUE(window.check(i)) << "Counter " << i << " should be accepted";
    }
}

TEST(AntiReplayWindowTest, DuplicateRejected)
{
    AntiReplayWindow window;
    EXPECT_TRUE(window.check(1));
    EXPECT_FALSE(window.check(1));

    EXPECT_TRUE(window.check(5));
    EXPECT_FALSE(window.check(5));
}

TEST(AntiReplayWindowTest, OutOfOrderWithinWindow)
{
    AntiReplayWindow window;
    EXPECT_TRUE(window.check(5));
    EXPECT_TRUE(window.check(3));
    EXPECT_TRUE(window.check(4));
    EXPECT_TRUE(window.check(1));
    EXPECT_TRUE(window.check(2));

    // All should be rejected now (duplicates).
    for (quint64 i = 1; i <= 5; ++i)
    {
        EXPECT_FALSE(window.check(i)) << "Counter " << i << " should be rejected as duplicate";
    }
}

TEST(AntiReplayWindowTest, TooOldRejected)
{
    AntiReplayWindow window;

    // Advance window far ahead.
    EXPECT_TRUE(window.check(AntiReplayWindow::kWindowSize + 100));

    // Counter 1 is now too old (outside the window).
    EXPECT_FALSE(window.check(1));

    // Counter just inside the window edge should still work.
    quint64 edge = AntiReplayWindow::kWindowSize + 100 - AntiReplayWindow::kWindowSize + 1;
    EXPECT_TRUE(window.check(edge));
}

TEST(AntiReplayWindowTest, LargeJump)
{
    AntiReplayWindow window;

    // Accept some packets.
    for (quint64 i = 1; i <= 10; ++i)
        EXPECT_TRUE(window.check(i));

    // Jump far ahead (larger than window).
    quint64 jump = AntiReplayWindow::kWindowSize * 2;
    EXPECT_TRUE(window.check(jump));

    // Old packets are all too old now.
    for (quint64 i = 1; i <= 10; ++i)
        EXPECT_FALSE(window.check(i));

    // Packets near the new maximum should work.
    EXPECT_TRUE(window.check(jump - 1));
    EXPECT_TRUE(window.check(jump - 100));
}

TEST(AntiReplayWindowTest, WindowEdge)
{
    AntiReplayWindow window;

    quint64 max = AntiReplayWindow::kWindowSize + 1;
    EXPECT_TRUE(window.check(max));

    // Exactly at window boundary (oldest valid).
    EXPECT_TRUE(window.check(2));

    // One before window boundary (too old).
    EXPECT_FALSE(window.check(1));
}

TEST(AntiReplayWindowTest, Reset)
{
    AntiReplayWindow window;
    EXPECT_TRUE(window.check(1));
    EXPECT_TRUE(window.check(2));

    window.reset();

    // After reset, same counters should be accepted again.
    EXPECT_TRUE(window.check(1));
    EXPECT_TRUE(window.check(2));
}

TEST(AntiReplayWindowTest, UnreliablePacketGaps)
{
    AntiReplayWindow window;

    // Simulate unreliable packets with gaps (some lost).
    EXPECT_TRUE(window.check(1));
    EXPECT_TRUE(window.check(3));   // 2 lost
    EXPECT_TRUE(window.check(7));   // 4,5,6 lost
    EXPECT_TRUE(window.check(10));  // 8,9 lost

    // Late arrival of a "lost" packet.
    EXPECT_TRUE(window.check(5));

    // But not a duplicate.
    EXPECT_FALSE(window.check(5));
}

TEST(AntiReplayWindowTest, StressSequential)
{
    AntiReplayWindow window;

    // Process a large number of sequential packets.
    for (quint64 i = 1; i <= 100000; ++i)
    {
        EXPECT_TRUE(window.check(i));
    }

    // Recent duplicates rejected.
    EXPECT_FALSE(window.check(100000));
    EXPECT_FALSE(window.check(99999));

    // Very old rejected.
    EXPECT_FALSE(window.check(1));
}

} // namespace base
