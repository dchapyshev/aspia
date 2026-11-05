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

#include <algorithm>
#include <chrono>
#include <cmath>

#include "base/logging.h"

namespace {

const qint64 kStartBandwidth = 128 * 1024;
const qint64 kMinBandwidth = 8 * 1024;
const qint64 kMaxBandwidth = 4800 * 1024;

// A probe later than expected by this much is given up.
const MilliSeconds kProbeExpireTime{ 10000 };

// Excess of the delay of a probe over the base delay. Below the first value the path is clear. The
// second value is the jitter of the excess of a probe. From the third value on a queue is acted on
// by the first probe that shows it, whatever else may hold the probe.
const MilliSeconds kClearExcess{ 50 };
const MilliSeconds kExcessNoise{ 30 };
const MilliSeconds kLargeExcess{ 1000 };

// Excess at which the agent lowers the frame rate.
const MilliSeconds kWarningExcess{ 300 };
const MilliSeconds kCriticalExcess{ 1000 };

// A queue drains within this time.
const MilliSeconds kDrainTime{ 1000 };

// The session sends a probe this often.
const MilliSeconds kProbeInterval{ 1000 };

// The queue a test of a rate may build until the next probe when the path passes no more than the
// bandwidth. The first for a path whose capacity is not known, the second for a test over the
// measured capacity. Such a test is made after the first interval, and after twice the previous one
// up to the second interval while it finds the same capacity again.
const MilliSeconds kGrowthDelay{ 200 };
const MilliSeconds kCapacityMargin{ 100 };
const Seconds kCapacityTestInterval{ 10 };
const Seconds kCapacityTestMaxInterval{ 60 };

// The probe goes out before the padding slice of its own interval, so a test is passed by this much
// of its rate.
const double kPassedFraction =
    1 - 2.0 * BandwidthController::kPaddingSlice.count() / kProbeInterval.count();

//--------------------------------------------------------------------------------------------------
double toSeconds(TimePoint::duration duration)
{
    return static_cast<double>(DurationCast<MilliSeconds>(duration).count()) / 1000;
}

} // namespace

//--------------------------------------------------------------------------------------------------
BandwidthControllerTcp::BandwidthControllerTcp()
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
BandwidthControllerTcp::~BandwidthControllerTcp()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::start()
{
    bandwidth_ = kStartBandwidth;
    LOG(INFO) << "Start bandwidth:" << (bandwidth_ / 1024) << "kB/s";
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::resetPath()
{
    // The queue of the old path does not hold the bandwidth down on the new one.
    if (bandwidth_)
        setBandwidth(capacity_ ? capacity_ : kStartBandwidth);

    stall_rate_ = 0;
    capacity_ = 0;
    capacity_time_.reset();
    capacity_test_interval_ = Seconds::zero();
    test_rate_ = 0;
    probe_rate_ = 0;
    queue_ = MilliSeconds::zero();
    expected_excess_ = MilliSeconds::zero();
    trusted_time_.reset();
    trusted_queue_ = MilliSeconds::zero();
    trusted_bytes_ = 0;
    suspect_ = false;
    base_delay_.reset();
    last_excess_ = MilliSeconds::zero();
    probe_send_time_.reset();
    previous_probe_time_.reset();
    probe_bytes_ = 0;
    sent_bytes_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool BandwidthControllerTcp::startProbe(TimePoint now)
{
    if (!bandwidth_ || probe_send_time_.has_value())
        return false;

    if (previous_probe_time_.has_value() &&
        now - *previous_probe_time_ < kProbeInterval - BandwidthController::kPaddingSlice / 2)
    {
        return false;
    }

    probe_send_time_ = now;
    probe_bytes_ = sent_bytes_;
    probe_rate_ = test_rate_;
    sent_bytes_ = 0;
    startTest(now);

    expected_excess_ = MilliSeconds::zero();
    if (capacity_ && previous_probe_time_.has_value())
    {
        const double seconds = toSeconds(queue_) + static_cast<double>(probe_bytes_) / capacity_ -
                               toSeconds(now - *previous_probe_time_);
        expected_excess_ = MilliSeconds(std::llround(std::max(seconds, 0.0) * 1000));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::finishProbe(TimePoint now)
{
    if (!probe_send_time_.has_value())
        return;

    const TimePoint send_time = *probe_send_time_;
    const MilliSeconds delay = DurationCast<MilliSeconds>(now - send_time);

    std::optional<double> interval;
    if (previous_probe_time_.has_value())
        interval = toSeconds(send_time - *previous_probe_time_);

    previous_probe_time_ = probe_send_time_;
    probe_send_time_.reset();

    // Neither a queue nor a retransmission brings a probe in faster than the path.
    if (!base_delay_.has_value() || delay < *base_delay_)
        base_delay_ = delay;

    MilliSeconds excess = delay - *base_delay_;
    const MilliSeconds growth = excess - last_excess_;
    const bool steady = std::chrono::abs(growth) <= kExcessNoise;

    // The time the queue had to drain since the previous probe at the rate the path passes at least.
    const qint64 rate = capacity_ ? capacity_ : bandwidth_;
    const double drain = interval.value_or(0) - static_cast<double>(probe_bytes_) / rate;

    // A queue grows or drains, and retransmissions do not hold two probes by the same time. An
    // excess that stays while the queue had to drain is a delay of the path that rose.
    if (excess >= kClearExcess && steady && drain > toSeconds(kExcessNoise))
    {
        base_delay_ = delay;
        excess = MilliSeconds::zero();
    }

    last_excess_ = excess;
    trusted_bytes_ += probe_bytes_;

    if (!interval.has_value() || !trusted_time_.has_value())
    {
        trustProbe(send_time, excess);
        return;
    }

    // The bytes sent since the trusted probe left the queue in the time since then plus the growth
    // of the queue at most, so the path passes at least that rate. It is the rate of the path when
    // the queue stood all that time and no retransmission held the probe. A rate over the capacity
    // has to be carried beyond the jitter of the excess to raise it.
    const double span = toSeconds(send_time - *trusted_time_) + toSeconds(excess - trusted_queue_);
    const double rate_bound = span > 0 ? trusted_bytes_ / span : 0;
    const double carried_rate = trusted_bytes_ / std::max(span + toSeconds(kExcessNoise), 0.001);

    if (excess < kClearExcess)
    {
        if (capacity_ && carried_rate > capacity_)
            capacity_ = std::llround(carried_rate);

        // The test passed when no queue grew over the one of the model.
        const bool passed = std::chrono::abs(excess - queue_) <= kExcessNoise;
        trustProbe(send_time, excess);
        finishTest(*interval, passed);
        return;
    }

    // The queue has to go on growing at the rate that built it for the next probe to show it.
    test_rate_ = probe_rate_;

    // A retransmission holds a probe for a round trip at least, so a probe the model foresaw within
    // half of it is not held by one, and neither is a probe earlier than foreseen.
    const MilliSeconds tolerance = std::max(kClearExcess, *base_delay_ / 2);
    const MilliSeconds residual = excess - expected_excess_;
    const bool foreseen = capacity_ && (residual <= -tolerance ||
                                        (expected_excess_ >= kClearExcess && residual < tolerance));

    if (foreseen)
    {
        // From a clear path the rate bound is only a lower bound.
        const bool exact = trusted_queue_ >= kClearExcess && rate_bound > 0;
        measureCapacity(exact ? rate_bound : std::max<double>(capacity_, carried_rate), excess, now);
        trustProbe(send_time, excess);
        return;
    }

    // A queue the model did not foresee is real when two probes in a row show it, retransmissions
    // do not hold them both. The path is narrower than the model, or its capacity is not known yet.
    if (suspect_ || excess >= kLargeExcess)
    {
        measureCapacity(rate_bound > 0 ? rate_bound : capacity_, excess, now);
        trustProbe(send_time, excess);
        return;
    }

    // One probe the model did not foresee is held by a retransmission or shows a narrower path, the
    // next probe tells which. Until then the queue is the one of the model.
    suspect_ = true;
    queue_ = expected_excess_;
    if (queue_ >= kClearExcess)
        drainQueue(queue_, now);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::onBytesSent(qint64 bytes)
{
    sent_bytes_ += bytes;
}

//--------------------------------------------------------------------------------------------------
// The slice is bounded so that a late timer does not send the padding as a burst the probe would
// take for a queue.
qint64 BandwidthControllerTcp::paddingDue(TimePoint now) const
{
    if (!test_rate_)
        return 0;

    const TimePoint since = probe_send_time_.value_or(previous_probe_time_.value_or(now));
    const qint64 due = std::llround(test_rate_ * toSeconds(now - since)) - sent_bytes_;
    return std::clamp<qint64>(due, 0, std::llround(test_rate_ * toSeconds(kPaddingSlice) * 2));
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::tick(TimePoint now)
{
    if (!probe_send_time_.has_value())
        return;

    const MilliSeconds excess = DurationCast<MilliSeconds>(now - *probe_send_time_) -
                                base_delay_.value_or(MilliSeconds::zero());

    // The excess of a probe that is given up is at least that.
    if (excess >= expected_excess_ + kProbeExpireTime)
    {
        LOG(WARNING) << "Probe expired";
        previous_probe_time_ = probe_send_time_;
        probe_send_time_.reset();
        last_excess_ = excess;
        trusted_bytes_ += probe_bytes_;
        suspect_ = true;
        queue_ = excess;
        test_rate_ = 0;
        return;
    }

    // The queue in front of the probe is longer than kDrainTime already, only a pause drains it in
    // that time.
    if (excess >= kLargeExcess && bandwidth_ > kMinBandwidth)
    {
        LOG(INFO) << "Probe stalled";
        test_rate_ = 0;
        stall_rate_ = std::max(stall_rate_, bandwidth_);
        setBandwidth(0);
    }
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Overflow::State BandwidthControllerTcp::overflowState(TimePoint now) const
{
    MilliSeconds excess = queue_;
    if (probe_send_time_.has_value())
    {
        // A probe this late shows a queue whatever else holds it (see tick()).
        const MilliSeconds in_flight = DurationCast<MilliSeconds>(now - *probe_send_time_) -
                                       base_delay_.value_or(MilliSeconds::zero());
        if (in_flight >= kLargeExcess)
            excess = std::max(excess, in_flight);
    }

    if (excess >= kCriticalExcess)
        return proto::desktop::Overflow::STATE_CRITICAL;
    else if (excess >= kWarningExcess)
        return proto::desktop::Overflow::STATE_WARNING;

    return proto::desktop::Overflow::STATE_NONE;
}

//--------------------------------------------------------------------------------------------------
// The window bounds the data in the path, not the drain of the queue, so a bandwidth lowered to
// drain it does not shrink the window under the rate the path carried.
qint64 BandwidthControllerTcp::windowSize(MilliSeconds path_delay) const
{
    const qint64 rate = std::max({ bandwidth_, test_rate_, capacity_, stall_rate_ });
    return std::llround(rate * toSeconds(base_delay_.value_or(path_delay) + kCapacityMargin));
}

//--------------------------------------------------------------------------------------------------
// The test of the interval that starts with the probe. The result of the last test is not in yet,
// so the next one is the step above it, taken back within a round trip when the probe finds the
// queue. On a held capacity a test is made now and then.
void BandwidthControllerTcp::startTest(TimePoint now)
{
    if (last_excess_ >= kClearExcess || bandwidth_ >= kMaxBandwidth)
        return;

    if (test_rate_)
        test_rate_ = std::llround(test_rate_ * (1 + toSeconds(kGrowthDelay)));
    else if (!capacity_ || bandwidth_ >= capacity_)
        test_rate_ = std::llround(bandwidth_ * (1 + toSeconds(kGrowthDelay)));
    else if (now - *capacity_time_ >= capacity_test_interval_)
        test_rate_ = std::llround(capacity_ * (1 + toSeconds(kCapacityMargin)));
    else
        return;

    test_rate_ = std::min(test_rate_, kMaxBandwidth);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::measureCapacity(double capacity, MilliSeconds queue, TimePoint now)
{
    LOG(INFO) << "Capacity:" << static_cast<qint64>(capacity / 1024) << "kB/s, queue:"
              << queue.count() << "ms";

    // A test over the capacity that found it again costs the path for nothing, the next one waits
    // longer.
    const bool same = capacity_ && probe_rate_ && capacity <= capacity_ * (1 + toSeconds(kCapacityMargin));
    capacity_test_interval_ = same ? std::min(capacity_test_interval_ * 2, kCapacityTestMaxInterval) :
                                     kCapacityTestInterval;

    capacity_ = std::llround(capacity);
    stall_rate_ = 0;
    drainQueue(queue, now);
}

//--------------------------------------------------------------------------------------------------
// The bandwidth under the capacity drains the queue in kDrainTime, and the test is over.
void BandwidthControllerTcp::drainQueue(MilliSeconds queue, TimePoint now)
{
    capacity_time_ = now;
    test_rate_ = 0;
    setBandwidth(capacity_ * (1 - toSeconds(queue) / toSeconds(kDrainTime)));
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::trustProbe(TimePoint send_time, MilliSeconds queue)
{
    trusted_time_ = send_time;
    trusted_queue_ = queue;
    trusted_bytes_ = 0;
    suspect_ = false;
    queue_ = queue;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::finishTest(double interval, bool steady)
{
    if (probe_rate_)
    {
        if (steady && probe_bytes_ >= kPassedFraction * probe_rate_ * interval)
        {
            setBandwidth(probe_rate_);
            if (bandwidth_ >= kMaxBandwidth)
                test_rate_ = 0;
        }
        else
        {
            test_rate_ = probe_rate_;
        }
    }

    if (capacity_)
        setBandwidth(std::max<double>(bandwidth_, capacity_ * (1 - toSeconds(kCapacityMargin))));
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerTcp::setBandwidth(double bandwidth)
{
    const qint64 before = bandwidth_;
    bandwidth_ = std::clamp(static_cast<qint64>(std::llround(bandwidth)), kMinBandwidth,
                            kMaxBandwidth);

    if (bandwidth_ / 1024 != before / 1024)
        LOG(INFO) << "Bandwidth:" << (before / 1024) << "->" << (bandwidth_ / 1024) << "kB/s";
}
