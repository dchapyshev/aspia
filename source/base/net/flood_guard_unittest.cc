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

#include "base/net/flood_guard.h"

#include <gtest/gtest.h>

#include <thread>

using namespace std::chrono_literals;

// Test-only adapter exposing the underlying per-address and per-pending checks, plus the
// internal map-size constant needed to drive the eviction-path test. Production callers reach
// FloodGuard via the single check() entry point that bundles both gates.
class FloodGuardTestPeer
{
public:
    static int maxTracked() { return FloodGuard::kMaxTracked; }

    static bool checkAddress(FloodGuard& guard, TimePoint now, const asio::ip::address& address)
    {
        return guard.checkAddress(now, address);
    }

    static bool checkPending(FloodGuard& guard, TimePoint now, int current_pending)
    {
        return guard.checkPending(now, current_pending);
    }
};

namespace {

asio::ip::address ipv4(const char* s)
{
    return asio::ip::make_address(s);
}

} // namespace

TEST(flood_guard_test, burst_then_reject)
{
    FloodGuard guard;
    guard.setRateLimit(1s, 5);
    const auto addr = ipv4("203.0.113.1");

    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr))
            << "attempt " << i;
    }

    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
}

TEST(flood_guard_test, refill_after_window)
{
    FloodGuard guard;
    guard.setRateLimit(1s, 2);
    const auto addr = ipv4("203.0.113.2");

    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));

    // Wait for full refill (window) + small margin.
    std::this_thread::sleep_for(1100ms);

    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
}

TEST(flood_guard_test, addresses_independent)
{
    FloodGuard guard;
    guard.setRateLimit(1s, 2);
    const auto a = ipv4("203.0.113.10");
    const auto b = ipv4("203.0.113.11");

    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), a));
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), a));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), a));

    // |b| has its own budget, untouched by |a|'s usage.
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), b));
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), b));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), b));
}

TEST(flood_guard_test, ipv4_and_ipv6_independent)
{
    FloodGuard guard;
    guard.setRateLimit(1s, 1);
    const auto v4 = ipv4("203.0.113.20");
    const auto v6 = asio::ip::make_address("2001:db8::1");

    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), v4));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), v4));

    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), v6));
    EXPECT_FALSE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), v6));
}

TEST(flood_guard_test, check_pending)
{
    FloodGuard guard;
    guard.setMaxPending(4);

    EXPECT_TRUE(FloodGuardTestPeer::checkPending(guard, Clock::now(), 0));
    EXPECT_TRUE(FloodGuardTestPeer::checkPending(guard, Clock::now(), 3));
    EXPECT_FALSE(FloodGuardTestPeer::checkPending(guard, Clock::now(), 4));
    EXPECT_FALSE(FloodGuardTestPeer::checkPending(guard, Clock::now(), 5));
    EXPECT_FALSE(FloodGuardTestPeer::checkPending(guard, Clock::now(), 100));
}

TEST(flood_guard_test, eviction_under_pressure)
{
    // Short window so the populating addresses fully refill during the sleep below and become
    // evictable on the next insert attempt.
    FloodGuard guard;
    guard.setRateLimit(1s, 2);

    const int max_tracked = FloodGuardTestPeer::maxTracked();

    // Fill the map up to the hard cap with distinct IPv4 addresses, each at one token spent.
    for (int i = 0; i < max_tracked; ++i)
    {
        const asio::ip::address addr = asio::ip::address_v4(0x0a000000u + i);
        EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), addr));
    }

    // Wait for those entries to fully refill (TAT <= now).
    std::this_thread::sleep_for(1100ms);

    // A new address arrives: the map is full but every existing entry is at capacity, so the
    // sweep evicts them and the new entry is admitted.
    const asio::ip::address fresh = asio::ip::make_address("192.168.0.1");
    EXPECT_TRUE(FloodGuardTestPeer::checkAddress(guard, Clock::now(), fresh));
}
