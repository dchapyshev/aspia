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

#include "host/bandwidth_controller_tcp.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace {

const qint64 kStartBandwidth = 128 * 1024;
const qint64 kMinBandwidth = 8 * 1024;
const qint64 kMaxBandwidth = 4800 * 1024;
const MilliSeconds kBaseDelay{ 300 };

// A retransmission holds a probe for more than a round trip.
const MilliSeconds kRetransmission{ 330 };

// A test over the bandwidth asks for this much more, a probe a second after the previous one.
const double kGrowth = 1.2;

// The capacity of the path in most tests, and how close the controller measures it.
const qint64 kCapacity = 1000 * 1024;
const qint64 kCapacityError = kCapacity / 200;

// What the session sent since the previous probe. PADDED is the bandwidth with the padding the
// controller asks for.
enum class Traffic { NONE, HALF, FULL, PADDED };

//--------------------------------------------------------------------------------------------------
// The path of the model: a queue in front of a link of a capacity. A probe waits behind the bytes
// sent before it.
class Path
{
public:
    explicit Path(qint64 capacity)
        : capacity_(capacity)
    {
        // Nothing
    }

    void setCapacity(qint64 capacity) { capacity_ = capacity; }

    // The session sent |bytes| over the second since the previous probe. Returns the delay of the
    // probe.
    MilliSeconds send(qint64 bytes)
    {
        queue_ = std::max(0.0, queue_ + static_cast<double>(bytes) / capacity_ - 1);
        return kBaseDelay + MilliSeconds(std::llround(queue_ * 1000));
    }

private:
    qint64 capacity_;
    double queue_ = 0;
};

//--------------------------------------------------------------------------------------------------
qint64 sendTraffic(BandwidthControllerTcp* controller, TimePoint now, Traffic traffic)
{
    qint64 bytes = 0;
    if (traffic == Traffic::HALF)
        bytes = controller->bandwidth() / 2;
    else if (traffic != Traffic::NONE)
        bytes = controller->bandwidth();
    controller->onBytesSent(bytes);

    if (traffic == Traffic::PADDED)
    {
        const qint64 padding = controller->paddingDue(now);
        controller->onBytesSent(padding);
        bytes += padding;
    }

    return bytes;
}

//--------------------------------------------------------------------------------------------------
// A probe goes out at |*now| and is acked |delay| later. Moves |*now| to the next second.
void probe(BandwidthControllerTcp* controller, TimePoint* now, MilliSeconds delay,
           Traffic traffic = Traffic::PADDED)
{
    sendTraffic(controller, *now, traffic);
    ASSERT_TRUE(controller->startProbe(*now));
    controller->finishProbe(*now + delay);
    *now += Seconds(1);
}

//--------------------------------------------------------------------------------------------------
// A probe over |path|, held |extra| longer by a retransmission.
void probe(BandwidthControllerTcp* controller, TimePoint* now, Path* path,
           Traffic traffic = Traffic::PADDED, MilliSeconds extra = MilliSeconds::zero())
{
    const qint64 bytes = sendTraffic(controller, *now, traffic);
    ASSERT_TRUE(controller->startProbe(*now));
    controller->finishProbe(*now + path->send(bytes) + extra);
    *now += Seconds(1);
}

//--------------------------------------------------------------------------------------------------
qint64 scaled(qint64 bandwidth, double factor)
{
    return std::llround(bandwidth * factor);
}

//--------------------------------------------------------------------------------------------------
// Climbs on |path| to its capacity, measures it by the queue of the test over it and holds the
// bandwidth at 0.9 of it.
void startAtCapacity(BandwidthControllerTcp* controller, TimePoint* now, Path* path)
{
    controller->start();
    for (int i = 0; i < 16; ++i)
        probe(controller, now, path);
    ASSERT_NEAR(controller->bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
// Holds the bandwidth until the test over the capacity starts.
void holdUntilTest(BandwidthControllerTcp* controller, TimePoint* now, Path* path)
{
    for (int i = 0; i < 20 && controller->paddingDue(*now) == 0; ++i)
        probe(controller, now, path);
    ASSERT_GT(controller->paddingDue(*now), 0);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, NoProbeBeforeStart)
{
    BandwidthControllerTcp controller;
    const TimePoint now = TimePoint() + Hours(1);

    EXPECT_EQ(controller.bandwidth(), 0);
    EXPECT_FALSE(controller.startProbe(now));
    EXPECT_EQ(controller.paddingDue(now), 0);

    controller.start();
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);
    EXPECT_TRUE(controller.startProbe(now));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, OneProbeInFlight)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    EXPECT_TRUE(controller.startProbe(now));
    EXPECT_FALSE(controller.startProbe(now + MilliSeconds(500)));

    controller.finishProbe(now + kBaseDelay);
    EXPECT_TRUE(controller.startProbe(now + Seconds(1)));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, ClearPathProvesTheNextRate)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    // The first probe starts the test, the next ones carry the padding and pass it.
    probe(&controller, &now, kBaseDelay);
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);

    probe(&controller, &now, kBaseDelay);
    EXPECT_EQ(controller.bandwidth(), scaled(kStartBandwidth, kGrowth));

    probe(&controller, &now, kBaseDelay);
    EXPECT_EQ(controller.bandwidth(), scaled(scaled(kStartBandwidth, kGrowth), kGrowth));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, PaddingFillsTheGapToTheTestRate)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    const qint64 test_rate = scaled(kStartBandwidth, kGrowth);

    // A second has passed with nothing sent, the slice is bounded to a fifth of the rate.
    EXPECT_EQ(controller.paddingDue(now), scaled(test_rate, 0.2));

    controller.onBytesSent(test_rate);
    EXPECT_EQ(controller.paddingDue(now), 0);
}

//--------------------------------------------------------------------------------------------------
// A session that sends no padding and less than the rate under test proves nothing.
TEST(BandwidthControllerTcpTest, NoGrowthWithoutPadding)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    for (int i = 0; i < 10; ++i)
        probe(&controller, &now, kBaseDelay, Traffic::NONE);
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);

    for (int i = 0; i < 10; ++i)
        probe(&controller, &now, kBaseDelay, Traffic::HALF);
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);
}

//--------------------------------------------------------------------------------------------------
// The climb passes rate after rate until a test over the capacity builds a queue. Without a
// capacity the first probe in the queue may be held by a retransmission as well, the second one
// measures it.
TEST(BandwidthControllerTcpTest, ClimbMeasuresTheCapacityByTwoProbes)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    controller.start();

    for (int i = 0; i < 12; ++i)
        probe(&controller, &now, &path);
    const qint64 passed = controller.bandwidth();

    probe(&controller, &now, &path);
    EXPECT_EQ(controller.bandwidth(), passed);

    // The bandwidth under the capacity drains the queue of 283 ms in a second, the test is over.
    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.717), kCapacityError);
    EXPECT_EQ(controller.paddingDue(now), 0);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
// The probes before and after a late one bound the rate of the path, the late one is skipped.
TEST(BandwidthControllerTcpTest, RetransmissionInTheFirstQueueIsSkipped)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    controller.start();

    for (int i = 0; i < 12; ++i)
        probe(&controller, &now, &path);

    probe(&controller, &now, &path, Traffic::PADDED, kRetransmission);
    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.717), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, RetransmittedProbeIsNotAQueue)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    for (int i = 0; i < 3; ++i)
        probe(&controller, &now, kBaseDelay);
    const qint64 before = controller.bandwidth();

    probe(&controller, &now, kBaseDelay + kRetransmission);
    EXPECT_EQ(controller.bandwidth(), before);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    // The next probe passes the rate the late one carried.
    probe(&controller, &now, kBaseDelay);
    EXPECT_EQ(controller.bandwidth(), scaled(before, kGrowth));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, LargeExcessIsMeasuredAtOnce)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    const qint64 capacity = std::llround(kStartBandwidth / 2.2);
    Path path(capacity);
    controller.start();

    // A queue longer than the drain time leaves nothing to send.
    probe(&controller, &now, &path, Traffic::NONE);
    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_EQ(controller.bandwidth(), kMinBandwidth);

    // The queue drains a second per second, the model foresaw what is left of it.
    probe(&controller, &now, &path, Traffic::NONE);
    EXPECT_NEAR(controller.bandwidth(), scaled(capacity, 0.8), 1);

    probe(&controller, &now, &path, Traffic::NONE);
    EXPECT_NEAR(controller.bandwidth(), scaled(capacity, 0.9), 1);
}

//--------------------------------------------------------------------------------------------------
// An idle session builds no queue, so an excess two probes show alike is the delay of the path.
TEST(BandwidthControllerTcpTest, StandingExcessOfAnIdlePathBecomesTheBase)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    probe(&controller, &now, kBaseDelay + MilliSeconds(200), Traffic::NONE);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    probe(&controller, &now, kBaseDelay + MilliSeconds(200), Traffic::NONE);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);

    // The window holds the round trip of the new base delay.
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)),
              scaled(scaled(kStartBandwidth, kGrowth), 0.6));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, RetransmittedIdleProbeKeepsTheBase)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    probe(&controller, &now, kBaseDelay + kRetransmission, Traffic::NONE);
    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)),
              scaled(scaled(kStartBandwidth, kGrowth), 0.4));
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, NoiseOfTheExcessIsNotAQueue)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    for (int i = 0; i < 5; ++i)
    {
        probe(&controller, &now, kBaseDelay);
        probe(&controller, &now, kBaseDelay + MilliSeconds(25));
    }

    EXPECT_GT(controller.bandwidth(), kStartBandwidth * 3);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, BandwidthHoldsUnderTheCapacity)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);

    const qint64 held = controller.bandwidth();
    for (int i = 0; i < 5; ++i)
    {
        probe(&controller, &now, &path);
        EXPECT_EQ(controller.bandwidth(), held);
        EXPECT_EQ(controller.paddingDue(now), 0);
    }
}

//--------------------------------------------------------------------------------------------------
// On a path that grew the test over the capacity is padded and passed, and the climb goes on.
TEST(BandwidthControllerTcpTest, StepOverTheCapacityIsProvedByPadding)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);

    path.setCapacity(kCapacity * 2);
    holdUntilTest(&controller, &now, &path);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 1.1), kCapacityError);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 1.1 * kGrowth), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
// The test brought the rate to 1.1 of the capacity, the queue of 100 ms is the one the model
// foresaw.
TEST(BandwidthControllerTcpTest, QueueOverTheCapacityIsMeasuredByTheFirstProbe)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);
    holdUntilTest(&controller, &now, &path);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
    EXPECT_EQ(controller.paddingDue(now), 0);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
// A retransmission holds the probe of the test over the capacity. The model drains the queue it
// foresaw, and the capacity stays.
TEST(BandwidthControllerTcpTest, RetransmissionOverTheCapacityChangesNothing)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);
    holdUntilTest(&controller, &now, &path);

    probe(&controller, &now, &path, Traffic::PADDED, kRetransmission);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
    EXPECT_EQ(controller.paddingDue(now), 0);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    probe(&controller, &now, &path);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);
}

//--------------------------------------------------------------------------------------------------
// The path narrows under the held bandwidth. One probe over the model may be late by itself, the
// second one measures the narrower path.
TEST(BandwidthControllerTcpTest, NarrowingIsMeasuredByTheSecondProbe)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);

    path.setCapacity(scaled(kCapacity, 0.8));
    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.9), kCapacityError);

    // The bandwidth under the capacity drains the queue of 250 ms in a second.
    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.8 * 0.75), kCapacityError);

    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.8 * 0.9), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, RetransmissionInTheNarrowingIsSkipped)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);

    path.setCapacity(scaled(kCapacity, 0.8));
    probe(&controller, &now, &path, Traffic::FULL, kRetransmission);
    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_NEAR(controller.bandwidth(), scaled(kCapacity, 0.8 * 0.75), kCapacityError);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, StallLeavesNothingToSend)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();
    probe(&controller, &now, kBaseDelay, Traffic::NONE);

    controller.onBytesSent(controller.bandwidth());
    ASSERT_TRUE(controller.startProbe(now));
    controller.tick(now + MilliSeconds(1000));
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);

    controller.tick(now + kBaseDelay + MilliSeconds(1000));
    EXPECT_EQ(controller.bandwidth(), kMinBandwidth);
    EXPECT_EQ(controller.paddingDue(now + MilliSeconds(1100)), 0);

    // The late ack measures the capacity, the next clear probe returns to it.
    controller.finishProbe(now + kBaseDelay + MilliSeconds(1480));
    EXPECT_EQ(controller.bandwidth(), kMinBandwidth);

    const qint64 capacity = std::llround(kStartBandwidth / 2.48);
    now += Seconds(2);
    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    EXPECT_NEAR(controller.bandwidth(), scaled(capacity, 0.9), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, StalledProbeExpires)
{
    BandwidthControllerTcp controller;
    const TimePoint now = TimePoint() + Hours(1);
    controller.start();

    ASSERT_TRUE(controller.startProbe(now));
    controller.tick(now + MilliSeconds(2000));
    EXPECT_FALSE(controller.startProbe(now + MilliSeconds(9000)));

    // The path is not back until a probe comes back.
    controller.tick(now + MilliSeconds(10000));
    EXPECT_TRUE(controller.startProbe(now + MilliSeconds(10000)));
    EXPECT_EQ(controller.overflowState(now + MilliSeconds(10000)),
              proto::desktop::Overflow::STATE_CRITICAL);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, OverflowStateFollowsTheQueue)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    // The first probe over the model may be late by itself, the second one shows the queue.
    path.setCapacity(scaled(kCapacity, 0.7));
    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);

    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_WARNING);

    probe(&controller, &now, &path, Traffic::FULL);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, OverflowStateCountsAStalledProbe)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();
    probe(&controller, &now, kBaseDelay, Traffic::NONE);

    // A retransmission may hold the probe this long, the stall is a queue whatever holds it.
    ASSERT_TRUE(controller.startProbe(now));
    EXPECT_EQ(controller.overflowState(now + kBaseDelay + MilliSeconds(500)),
              proto::desktop::Overflow::STATE_NONE);
    EXPECT_EQ(controller.overflowState(now + kBaseDelay + MilliSeconds(1100)),
              proto::desktop::Overflow::STATE_CRITICAL);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, WindowSizeFollowsTheBandwidthAndDelay)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    // The window holds 100 ms of queue over the delay of the path. Until a probe has measured the
    // delay, the one of the transport is taken. Under a test the window is that of the test.
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)), scaled(kStartBandwidth, 0.2));

    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)),
              scaled(scaled(kStartBandwidth, kGrowth), 0.4));
}

//--------------------------------------------------------------------------------------------------
// The bandwidth lowered to drain a queue does not shrink the window, or the queue would not drain.
TEST(BandwidthControllerTcpTest, WindowSizeStaysAtTheCapacityUnderTheDrain)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    probe(&controller, &now, kBaseDelay, Traffic::NONE);
    probe(&controller, &now, kBaseDelay + MilliSeconds(1200), Traffic::FULL);
    ASSERT_EQ(controller.bandwidth(), kMinBandwidth);

    const qint64 capacity = std::llround(kStartBandwidth / 2.2);
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)), scaled(capacity, 0.4));
}

//--------------------------------------------------------------------------------------------------
// A stall before the capacity is measured does not shrink the window either.
TEST(BandwidthControllerTcpTest, WindowSizeStaysAtTheBandwidthUnderAStall)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();
    probe(&controller, &now, kBaseDelay, Traffic::NONE);

    controller.onBytesSent(controller.bandwidth());
    ASSERT_TRUE(controller.startProbe(now));
    controller.tick(now + kBaseDelay + MilliSeconds(1000));
    ASSERT_EQ(controller.bandwidth(), kMinBandwidth);
    EXPECT_EQ(controller.windowSize(MilliSeconds(100)), scaled(kStartBandwidth, 0.4));

    // The late ack measures the capacity, and the window follows it.
    controller.finishProbe(now + kBaseDelay + MilliSeconds(1480));
    const qint64 capacity = std::llround(kStartBandwidth / 2.48);
    EXPECT_NEAR(controller.windowSize(MilliSeconds(100)), scaled(capacity, 0.4), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, ResetPathForgetsBaseDelay)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();
    probe(&controller, &now, kBaseDelay, Traffic::NONE);

    // The new path is slower. Its delay is not an excess over the delay of the old one.
    controller.resetPath();
    probe(&controller, &now, kBaseDelay * 2, Traffic::NONE);
    probe(&controller, &now, kBaseDelay * 2, Traffic::NONE);
    probe(&controller, &now, kBaseDelay * 2, Traffic::NONE);
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);
    EXPECT_EQ(controller.overflowState(now), proto::desktop::Overflow::STATE_NONE);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, ResetPathStartsFromTheCapacity)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    Path path(kCapacity);
    startAtCapacity(&controller, &now, &path);

    controller.resetPath();
    EXPECT_NEAR(controller.bandwidth(), kCapacity, kCapacityError);
    EXPECT_EQ(controller.paddingDue(now), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, ResetPathWithoutCapacityStartsOver)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    for (int i = 0; i < 3; ++i)
        probe(&controller, &now, kBaseDelay);
    ASSERT_GT(controller.bandwidth(), kStartBandwidth);

    controller.resetPath();
    EXPECT_EQ(controller.bandwidth(), kStartBandwidth);
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerTcpTest, BandwidthStaysWithinBounds)
{
    BandwidthControllerTcp controller;
    TimePoint now = TimePoint() + Hours(1);
    controller.start();

    for (int i = 0; i < 30; ++i)
        probe(&controller, &now, kBaseDelay);
    EXPECT_EQ(controller.bandwidth(), kMaxBandwidth);
    EXPECT_EQ(controller.paddingDue(now), 0);

    for (int i = 0; i < 20; ++i)
        probe(&controller, &now, kBaseDelay + MilliSeconds(1000 + i * 100), Traffic::FULL);
    EXPECT_EQ(controller.bandwidth(), kMinBandwidth);
}
