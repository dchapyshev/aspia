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

namespace {

// Convenience: emulates the legacy single-call semantics for tests that don't
// care about the two-phase pattern.
bool checkAndCommit(AntiReplayWindow& window, quint64 counter)
{
    if (!window.precheck(counter))
        return false;
    window.commit(counter);
    return true;
}

} // namespace

TEST(AntiReplayWindowTest, ZeroCounterRejected)
{
    AntiReplayWindow window;
    EXPECT_FALSE(window.precheck(0));
}

TEST(AntiReplayWindowTest, SequentialAccepted)
{
    AntiReplayWindow window;
    for (quint64 i = 1; i <= 100; ++i)
    {
        EXPECT_TRUE(checkAndCommit(window, i)) << "Counter " << i << " should be accepted";
    }
}

TEST(AntiReplayWindowTest, DuplicateRejected)
{
    AntiReplayWindow window;
    EXPECT_TRUE(checkAndCommit(window, 1));
    EXPECT_FALSE(window.precheck(1));

    EXPECT_TRUE(checkAndCommit(window, 5));
    EXPECT_FALSE(window.precheck(5));
}

TEST(AntiReplayWindowTest, OutOfOrderWithinWindow)
{
    AntiReplayWindow window;
    EXPECT_TRUE(checkAndCommit(window, 5));
    EXPECT_TRUE(checkAndCommit(window, 3));
    EXPECT_TRUE(checkAndCommit(window, 4));
    EXPECT_TRUE(checkAndCommit(window, 1));
    EXPECT_TRUE(checkAndCommit(window, 2));

    // All should be rejected now (duplicates).
    for (quint64 i = 1; i <= 5; ++i)
    {
        EXPECT_FALSE(window.precheck(i)) << "Counter " << i << " should be rejected as duplicate";
    }
}

TEST(AntiReplayWindowTest, TooOldRejected)
{
    AntiReplayWindow window;

    // Advance window far ahead.
    EXPECT_TRUE(checkAndCommit(window, AntiReplayWindow::kWindowSize + 100));

    // Counter 1 is now too old (outside the window).
    EXPECT_FALSE(window.precheck(1));

    // Counter just inside the window edge should still work.
    quint64 edge = AntiReplayWindow::kWindowSize + 100 - AntiReplayWindow::kWindowSize + 1;
    EXPECT_TRUE(checkAndCommit(window, edge));
}

TEST(AntiReplayWindowTest, LargeJump)
{
    AntiReplayWindow window;

    // Accept some packets.
    for (quint64 i = 1; i <= 10; ++i)
        EXPECT_TRUE(checkAndCommit(window, i));

    // Jump far ahead (larger than window).
    quint64 jump = AntiReplayWindow::kWindowSize * 2;
    EXPECT_TRUE(checkAndCommit(window, jump));

    // Old packets are all too old now.
    for (quint64 i = 1; i <= 10; ++i)
        EXPECT_FALSE(window.precheck(i));

    // Packets near the new maximum should work.
    EXPECT_TRUE(checkAndCommit(window, jump - 1));
    EXPECT_TRUE(checkAndCommit(window, jump - 100));
}

TEST(AntiReplayWindowTest, WindowEdge)
{
    AntiReplayWindow window;

    quint64 max = AntiReplayWindow::kWindowSize + 1;
    EXPECT_TRUE(checkAndCommit(window, max));

    // Exactly at window boundary (oldest valid).
    EXPECT_TRUE(checkAndCommit(window, 2));

    // One before window boundary (too old).
    EXPECT_FALSE(window.precheck(1));
}

TEST(AntiReplayWindowTest, Reset)
{
    AntiReplayWindow window;
    EXPECT_TRUE(checkAndCommit(window, 1));
    EXPECT_TRUE(checkAndCommit(window, 2));

    window.reset();

    // After reset, same counters should be accepted again.
    EXPECT_TRUE(checkAndCommit(window, 1));
    EXPECT_TRUE(checkAndCommit(window, 2));
}

TEST(AntiReplayWindowTest, UnreliablePacketGaps)
{
    AntiReplayWindow window;

    // Simulate unreliable packets with gaps (some lost).
    EXPECT_TRUE(checkAndCommit(window, 1));
    EXPECT_TRUE(checkAndCommit(window, 3));   // 2 lost
    EXPECT_TRUE(checkAndCommit(window, 7));   // 4,5,6 lost
    EXPECT_TRUE(checkAndCommit(window, 10));  // 8,9 lost

    // Late arrival of a "lost" packet.
    EXPECT_TRUE(checkAndCommit(window, 5));

    // But not a duplicate.
    EXPECT_FALSE(window.precheck(5));
}

TEST(AntiReplayWindowTest, StressSequential)
{
    AntiReplayWindow window;

    // Process a large number of sequential packets.
    for (quint64 i = 1; i <= 100000; ++i)
    {
        EXPECT_TRUE(checkAndCommit(window, i));
    }

    // Recent duplicates rejected.
    EXPECT_FALSE(window.precheck(100000));
    EXPECT_FALSE(window.precheck(99999));

    // Very old rejected.
    EXPECT_FALSE(window.precheck(1));
}

TEST(AntiReplayWindowTest, PrecheckDoesNotMutate)
{
    AntiReplayWindow window;

    // precheck() called many times without commit() must not advance the window.
    for (int i = 0; i < 100; ++i)
        EXPECT_TRUE(window.precheck(42));

    // After all those prechecks the real packet with counter 42 must still be accepted.
    EXPECT_TRUE(checkAndCommit(window, 42));
    // And only then become a duplicate.
    EXPECT_FALSE(window.precheck(42));
}

TEST(AntiReplayWindowTest, NoPoisoningByUnauthenticatedHighCounter)
{
    AntiReplayWindow window;

    // Legitimate traffic flows for a while.
    for (quint64 i = 1; i <= 1000; ++i)
        EXPECT_TRUE(checkAndCommit(window, i));

    // Attacker injects a spoofed packet with a huge counter. In the new API the channel
    // would call precheck() first, then fail authentication, then NOT call commit(). Even
    // though precheck returns true, the window must stay where it was.
    constexpr quint64 kSpoofed = 1ULL << 60;
    EXPECT_TRUE(window.precheck(kSpoofed));
    // commit() intentionally NOT called - simulates failed authentication.

    // Subsequent legitimate packets must continue to be accepted.
    for (quint64 i = 1001; i <= 1100; ++i)
        EXPECT_TRUE(checkAndCommit(window, i)) << "Counter " << i << " must survive poisoning";

    // Old packets within the window are still rejected as duplicates.
    EXPECT_FALSE(window.precheck(1000));
}
