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

#include "host/capture_scheduler.h"

#include <gtest/gtest.h>

#include <QThread>

//--------------------------------------------------------------------------------------------------
TEST(CaptureSchedulerTest, DefaultIntervalIs25Fps)
{
    CaptureScheduler scheduler;

    EXPECT_EQ(scheduler.updateInterval(), MilliSeconds(40));
    EXPECT_EQ(scheduler.fps(), 25);
    EXPECT_FALSE(scheduler.isInProgress());
}

//--------------------------------------------------------------------------------------------------
TEST(CaptureSchedulerTest, FpsSetsInterval)
{
    CaptureScheduler scheduler;

    scheduler.setFps(1);
    EXPECT_EQ(scheduler.updateInterval(), MilliSeconds(1000));
    EXPECT_EQ(scheduler.fps(), 1);

    scheduler.setFps(10);
    EXPECT_EQ(scheduler.updateInterval(), MilliSeconds(100));
    EXPECT_EQ(scheduler.fps(), 10);

    scheduler.setFps(50);
    EXPECT_EQ(scheduler.updateInterval(), MilliSeconds(20));
    EXPECT_EQ(scheduler.fps(), 50);
}

//--------------------------------------------------------------------------------------------------
// The interval is whole milliseconds, so a rate above 1000 frames per second is capped there.
TEST(CaptureSchedulerTest, IntervalIsAtLeastOneMillisecond)
{
    CaptureScheduler scheduler;

    scheduler.setFps(5000);
    EXPECT_EQ(scheduler.updateInterval(), MilliSeconds(1));
    EXPECT_EQ(scheduler.fps(), 1000);
}

//--------------------------------------------------------------------------------------------------
TEST(CaptureSchedulerTest, BeginCaptureMarksProgress)
{
    CaptureScheduler scheduler;

    EXPECT_FALSE(scheduler.isInProgress());

    scheduler.onBeginCapture();
    EXPECT_TRUE(scheduler.isInProgress());

    scheduler.nextCaptureDelay();
    EXPECT_FALSE(scheduler.isInProgress());
}

//--------------------------------------------------------------------------------------------------
TEST(CaptureSchedulerTest, DelayNeverExceedsInterval)
{
    CaptureScheduler scheduler;
    scheduler.setFps(10);

    scheduler.onBeginCapture();

    const MilliSeconds delay = scheduler.nextCaptureDelay();
    EXPECT_GE(delay, MilliSeconds(0));
    EXPECT_LE(delay, scheduler.updateInterval());
}

//--------------------------------------------------------------------------------------------------
// A capture that took longer than the interval leaves no time to wait: the next one starts at once.
TEST(CaptureSchedulerTest, SlowCaptureGetsNoDelay)
{
    CaptureScheduler scheduler;
    scheduler.setFps(50);

    scheduler.onBeginCapture();
    QThread::sleep(MilliSeconds(60));

    EXPECT_EQ(scheduler.nextCaptureDelay(), MilliSeconds(0));
}
