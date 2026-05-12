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

namespace {

asio::ip::address ipv4(const char* s)
{
    return asio::ip::make_address(s);
}

} // namespace

TEST(flood_guard_test, burst_then_reject)
{
    FloodGuard guard(1s, 5, 1000);
    const auto addr = ipv4("203.0.113.1");

    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), addr)) << "attempt " << i;

    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), addr));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), addr));
}

TEST(flood_guard_test, refill_after_window)
{
    FloodGuard guard(1s, 2, 1000);
    const auto addr = ipv4("203.0.113.2");

    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), addr));
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), addr));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), addr));

    // Wait for full refill (window) + small margin.
    std::this_thread::sleep_for(1100ms);

    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), addr));
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), addr));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), addr));
}

TEST(flood_guard_test, addresses_independent)
{
    FloodGuard guard(1s, 2, 1000);
    const auto a = ipv4("203.0.113.10");
    const auto b = ipv4("203.0.113.11");

    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), a));
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), a));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), a));

    // |b| has its own budget, untouched by |a|'s usage.
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), b));
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), b));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), b));
}

TEST(flood_guard_test, ipv4_and_ipv6_independent)
{
    FloodGuard guard(1s, 1, 1000);
    const auto v4 = ipv4("203.0.113.20");
    const auto v6 = asio::ip::make_address("2001:db8::1");

    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), v4));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), v4));

    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), v6));
    EXPECT_FALSE(guard.checkAddress(FloodGuard::Clock::now(), v6));
}

TEST(flood_guard_test, check_pending)
{
    FloodGuard guard(/*window=*/1s, /*max_per_window=*/100, /*max_pending=*/4);

    EXPECT_TRUE(guard.checkPending(FloodGuard::Clock::now(), 0));
    EXPECT_TRUE(guard.checkPending(FloodGuard::Clock::now(), 3));
    EXPECT_FALSE(guard.checkPending(FloodGuard::Clock::now(), 4));
    EXPECT_FALSE(guard.checkPending(FloodGuard::Clock::now(), 5));
    EXPECT_FALSE(guard.checkPending(FloodGuard::Clock::now(), 100));
}

TEST(flood_guard_test, eviction_under_pressure)
{
    // Tiny tracked size and tiny window so refilled entries are evictable.
    FloodGuard guard(/*window=*/1s, /*max_per_window=*/2, /*max_pending=*/1000, /*max_tracked=*/4);

    // Fill the map with 4 distinct addresses, each at 1 token left.
    const char* addrs[] = { "203.0.113.100", "203.0.113.101", "203.0.113.102", "203.0.113.103" };
    for (const char* a : addrs)
        EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), ipv4(a)));

    // Wait for those entries to fully refill.
    std::this_thread::sleep_for(1100ms);

    // A new address arrives: the map is full but the existing entries are at capacity, so the
    // sweep can evict them. The new entry is admitted.
    EXPECT_TRUE(guard.checkAddress(FloodGuard::Clock::now(), ipv4("203.0.113.200")));
}
