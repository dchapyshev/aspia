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

#include "host/connection_log_throttle.h"

#include <gtest/gtest.h>

//--------------------------------------------------------------------------------------------------
TEST(ConnectionLogThrottleTest, FirstLoggedThenCountedPerSource)
{
    ConnectionLogThrottle throttle(MilliSeconds(Minutes(10)));
    const TimePoint t0 = Clock::now();
    int suppressed = -1;

    EXPECT_TRUE(throttle.shouldLog("10.0.0.1", t0, &suppressed));
    EXPECT_EQ(suppressed, 0);

    for (int i = 1; i <= 5; ++i)
        EXPECT_FALSE(throttle.shouldLog("10.0.0.1", t0 + Seconds(i), &suppressed));

    // Another source is independent.
    EXPECT_TRUE(throttle.shouldLog("10.0.0.2", t0 + Seconds(6), &suppressed));
    EXPECT_EQ(suppressed, 0);

    // After the interval the next one is logged with the count of the suppressed ones.
    EXPECT_TRUE(throttle.shouldLog("10.0.0.1", t0 + Minutes(10), &suppressed));
    EXPECT_EQ(suppressed, 5);
    EXPECT_FALSE(throttle.shouldLog("10.0.0.1", t0 + Minutes(11), &suppressed));
}

//--------------------------------------------------------------------------------------------------
TEST(ConnectionLogThrottleTest, MemoryIsBounded)
{
    ConnectionLogThrottle throttle(MilliSeconds(Minutes(10)));
    const TimePoint t0 = Clock::now();
    int suppressed = 0;

    for (int i = 0; i < ConnectionLogThrottle::kMaxSources + 10; ++i)
        EXPECT_TRUE(throttle.shouldLog(QString::number(i), t0, &suppressed));

    // Overflow starts over: logging more is the safe direction.
    EXPECT_TRUE(throttle.shouldLog(QStringLiteral("0"), t0 + Seconds(1), &suppressed));
}
