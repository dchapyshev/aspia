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

#include "host/bandwidth_controller_udp.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"

namespace {

const qint64 kMinBandwidth = 8 * 1024;
const qint64 kMaxBandwidth = 4800 * 1024;

// The share of the estimate that is published, the rate control of the encoder overshoots by the
// rest. The queue leaves in the drain time.
const double kBandwidthShare = 1.0 / 1.15;
const double kQueueDrainTime = 1.0;

// A delay of the path moves the acknowledgements, so it changes the mean over the busy time by no
// more than its share of that time. At twice the measured delay of 201 ms this stays in the share
// of the estimate that is not published.
const double kRateAveragingTime = 4.0;

// The slices of a standing queue that give the bound of a falling rate, and the rate under the
// estimate that sets it. A delay of the path under 400 ms does not reach it in that time.
const int kRecentRateSlices = 20;
const double kRecentRateMargin = 1.25;

// A publication down does not wait for the tick when it is this far under the published bandwidth.
const double kFastDecreaseShare = 0.9;

// The excess at which the agent lowers the frame rate.
const MilliSeconds kWarningQueue{ 300 };
const MilliSeconds kCriticalQueue{ 1000 };

// The session sends a probe this often, only for the acknowledgement it brings after a frame.
const MilliSeconds kProbeInterval{ 950 };

// A window holds a round trip of the rate twice, so that half of the time is enough to send it.
const qint64 kMaxWindow = 512 * 1024;
const MilliSeconds kWindowMargin{ 100 };

// The lowest round trip is forgotten by this time and the peak by this one.
const Seconds kRoundTripPeriod{ 10 };
const Seconds kPeakPeriod{ 60 };

// The rate of a test over the estimate. The first is for a rate that goes on growing.
const double kGrowingGain = 1.5;
const double kHoldingGain = 1.25;
const int kMinTestSlices = 4;

// When the next test is made.
const Seconds kFirstTestDelay{ 1 };
const Seconds kRepeatedTestDelay{ 2 };
const Seconds kTestInterval{ 10 };
const Seconds kMaxTestInterval{ 60 };

// A bound that failed this many tests in a row replaces the estimate.
const int kRecentRateFailures = 2;

//--------------------------------------------------------------------------------------------------
double toSeconds(TimePoint::duration duration)
{
    return std::chrono::duration<double>(duration).count();
}

//--------------------------------------------------------------------------------------------------
int slicesOf(double seconds)
{
    const double slice = toSeconds(BandwidthController::kPaddingSlice);
    return static_cast<int>(std::floor(seconds / slice));
}

} // namespace

//--------------------------------------------------------------------------------------------------
BandwidthControllerUdp::BandwidthControllerUdp(qint64 bandwidth)
    : delivery_rate_(static_cast<double>(bandwidth)),
      peak_rate_(static_cast<double>(bandwidth)),
      test_gain_(kGrowingGain),
      test_interval_(kTestInterval),
      bandwidth_(bandwidth)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
BandwidthControllerUdp::~BandwidthControllerUdp()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::start()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool BandwidthControllerUdp::startProbe(TimePoint now)
{
    if (last_probe_time_.has_value() && now - *last_probe_time_ < kProbeInterval)
        return false;

    last_probe_time_ = now;
    return true;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::finishProbe(TimePoint /* now */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::onBytesSent(qint64 bytes)
{
    slice_bytes_ += bytes;
}

//--------------------------------------------------------------------------------------------------
qint64 BandwidthControllerUdp::paddingDue(TimePoint /* now */) const
{
    return padding_due_;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::tick(TimePoint /* now */)
{
    setBandwidth(calculateBandwidth());
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Overflow::State BandwidthControllerUdp::overflowState(TimePoint /* now */) const
{
    const double seconds = standingQueueBytes() / std::max(usableRate(), 1.0);

    if (seconds >= toSeconds(kCriticalQueue))
        return proto::desktop::Overflow::STATE_CRITICAL;
    else if (seconds >= toSeconds(kWarningQueue))
        return proto::desktop::Overflow::STATE_WARNING;

    return proto::desktop::Overflow::STATE_NONE;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::onCounters(TimePoint now, const Counters& counters)
{
    const qint64 data_bytes = slice_bytes_ - padding_due_;
    slice_bytes_ = 0;
    padding_due_ = 0;

    updateRoundTrip(now, counters.round_trip);
    addCounters(now, counters);

    const qint64 backlog = counters.sent - counters.transmitted;
    queued_slices_ = backlog > 0 ? queued_slices_ + 1 : 0;

    const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
    const double path_bytes = usableRate() * round_trip;
    const double in_path = std::max(static_cast<double>(counters.in_flight) - path_bytes, 0.0);

    queue_history_[queue_history_size_ % kQueueHistorySize] = backlog + std::llround(in_path);
    ++queue_history_size_;

    updateDeliveryRate();
    updateRecentRate();
    updatePeakRate(now);
    runRateTest(now, data_bytes);

    const qint64 target = calculateBandwidth();
    if (target < bandwidth_ * kFastDecreaseShare)
        setBandwidth(target);
}

//--------------------------------------------------------------------------------------------------
qint64 BandwidthControllerUdp::windowSize(MilliSeconds path_delay) const
{
    const double round_trip = toSeconds(min_round_trip_.value_or(path_delay));
    const double seconds = std::max(2 * round_trip, round_trip + toSeconds(kWindowMargin));
    const double top = std::max({ peak_rate_, test_rate_, delivery_rate_ });

    return std::min(std::llround(top * seconds), kMaxWindow);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::addCounters(TimePoint now, const Counters& counters)
{
    Sample& sample = counters_history_[counters_history_size_ % kCountersHistorySize];

    sample.time = now;
    sample.sent = counters.sent;
    sample.transmitted = counters.transmitted;
    sample.delivered = counters.delivered;

    ++counters_history_size_;
}

//--------------------------------------------------------------------------------------------------
const BandwidthControllerUdp::Sample& BandwidthControllerUdp::countersAt(int slices_ago) const
{
    return counters_history_[(counters_history_size_ - 1 - slices_ago) % kCountersHistorySize];
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::updateRoundTrip(TimePoint now, MilliSeconds round_trip)
{
    if (round_trip <= MilliSeconds::zero())
        return;

    if (!min_round_trip_time_.has_value())
    {
        min_round_trip_time_ = now;
        current_min_round_trip_ = round_trip;
        previous_min_round_trip_ = round_trip;
    }

    current_min_round_trip_ = std::min(current_min_round_trip_, round_trip);

    if (now - *min_round_trip_time_ >= kRoundTripPeriod)
    {
        previous_min_round_trip_ = current_min_round_trip_;
        current_min_round_trip_ = round_trip;
        min_round_trip_time_ = now;
    }

    min_round_trip_ = std::min(previous_min_round_trip_, current_min_round_trip_);
}

//--------------------------------------------------------------------------------------------------
// A slice is busy when all that went out by its end was queued at its start, so the queue did not
// run out in it. The acknowledgements of the slice come a round trip later. The delivery after the
// last busy slice belongs to it too, without it the busy time misses the burst after a delay.
void BandwidthControllerUdp::updateDeliveryRate()
{
    const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
    const int shift = std::min(slicesOf(round_trip), kRecentRateSlices);

    if (counters_history_size_ < shift + 2 || counters_history_size_ < 2)
        return;

    const Sample& start = countersAt(shift + 1);
    const Sample& end = countersAt(shift);
    const Sample& previous = countersAt(1);
    const Sample& last = countersAt(0);

    const double seconds = toSeconds(last.time - previous.time);
    const qint64 delivered = last.delivered - previous.delivered;

    if (seconds <= 0)
        return;

    if (start.sent > start.transmitted && end.transmitted < start.sent)
    {
        last_busy_byte_ = std::max(last_busy_byte_, end.transmitted);
        addRateSample(delivered / seconds, seconds);
    }
    else if (previous.delivered < last_busy_byte_)
    {
        if (last.delivered < last_busy_byte_)
        {
            addRateSample(delivered / seconds, seconds);
        }
        else
        {
            const double bytes = static_cast<double>(last_busy_byte_ - previous.delivered);
            const double part = seconds * bytes / static_cast<double>(delivered);
            addRateSample(bytes / part, part);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::addRateSample(double rate, double seconds)
{
    busy_time_ += seconds;

    if (!delivery_rate_measured_)
    {
        delivery_rate_measured_ = true;
        delivery_rate_ = rate;
    }
    else
    {
        const double weight = busy_time_ < kRateAveragingTime ?
            seconds / busy_time_ : 1 - std::exp(-seconds / kRateAveragingTime);
        delivery_rate_ += weight * (rate - delivery_rate_);
    }

    delivery_rate_ = std::clamp<double>(delivery_rate_, kMinBandwidth, kMaxBandwidth);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::updateRecentRate()
{
    if (queued_slices_ < kRecentRateSlices || counters_history_size_ <= kRecentRateSlices)
        return;

    const Sample& first = countersAt(kRecentRateSlices);
    const Sample& last = countersAt(0);

    const double delivered = static_cast<double>(last.delivered - first.delivered);
    const double measured = delivered / toSeconds(last.time - first.time);

    if (measured >= delivery_rate_ / kRecentRateMargin)
    {
        recent_rate_.reset();
        recent_rate_failures_ = 0;
        return;
    }

    if (!recent_rate_.has_value())
    {
        next_test_time_ = last.time;
        test_gain_ = kHoldingGain;
    }

    recent_rate_ = std::max<double>(measured, kMinBandwidth);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::updatePeakRate(TimePoint now)
{
    if (delivery_rate_ >= peak_rate_ || !peak_rate_time_.has_value() ||
        now - *peak_rate_time_ > kPeakPeriod)
    {
        peak_rate_ = delivery_rate_;
        peak_rate_time_ = now;
    }

    if (delivery_rate_ < 0.9 * peak_rate_ && test_interval_ > kTestInterval)
    {
        test_interval_ = kTestInterval;
        next_test_time_ = std::min(next_test_time_.value_or(now + kTestInterval),
                                   now + kTestInterval);
    }
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::runRateTest(TimePoint now, qint64 data_bytes)
{
    if (test_slices_left_ > 0)
    {
        const double slice = toSeconds(kPaddingSlice);
        padding_due_ = std::max<qint64>(std::llround(test_rate_ * slice) - data_bytes, 0);
        --test_slices_left_;

        if (test_slices_left_ == 0)
        {
            const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
            const auto shift = kPaddingSlice * slicesOf(round_trip);

            test_measure_start_ = *test_measure_start_ + shift;
            test_result_time_ = now + kPaddingSlice + shift;
        }
        return;
    }

    if (test_result_time_.has_value())
    {
        if (now < *test_result_time_)
            return;

        finishRateTest(now);
    }

    if (!next_test_time_.has_value())
        next_test_time_ = now + kFirstTestDelay;

    const qint64 queued = countersAt(0).sent - countersAt(0).transmitted;
    if (now < *next_test_time_ || queued > 0 || delivery_rate_ >= kMaxBandwidth)
        return;

    startRateTest(now);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::finishRateTest(TimePoint now)
{
    test_result_time_.reset();

    int first = std::min(counters_history_size_, kCountersHistorySize) - 1;
    while (first > 0 && countersAt(first).time < *test_measure_start_)
        --first;

    double got = 0;
    const Sample& start = countersAt(first);
    const Sample& last = countersAt(0);
    if (last.time > start.time)
    {
        const double delivered = static_cast<double>(last.delivered - start.delivered);
        got = delivered / toSeconds(last.time - start.time);
    }

    const double result = std::min(got, test_rate_);
    const bool passed = result >= test_base_rate_ * (1 + (test_gain_ - 1) / 2);

    test_rate_ = 0;
    applyRateTestResult(result, passed);

    if (passed)
    {
        next_test_time_ = now;
        test_gain_ = kGrowingGain;
        test_interval_ = kTestInterval;
        test_retried_ = false;
    }
    else if (recent_rate_.has_value())
    {
        next_test_time_ = now + kRepeatedTestDelay;
        test_gain_ = kHoldingGain;
    }
    else if (!test_retried_)
    {
        test_retried_ = true;
        next_test_time_ = now + kRepeatedTestDelay;
    }
    else
    {
        test_retried_ = false;
        next_test_time_ = now + test_interval_;
        test_interval_ = std::min(test_interval_ * 2, kMaxTestInterval);
        test_gain_ = kHoldingGain;
    }
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::startRateTest(TimePoint now)
{
    const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
    const double slice = toSeconds(kPaddingSlice);

    test_base_rate_ = usableRate();
    test_rate_ = std::min<double>(test_base_rate_ * test_gain_, kMaxBandwidth);
    test_slices_left_ = std::max(kMinTestSlices, slicesOf(2 * round_trip + slice) + 1);
    test_measure_start_ = now + kPaddingSlice;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::applyRateTestResult(double rate, bool passed)
{
    if (recent_rate_.has_value())
    {
        if (passed)
        {
            recent_rate_failures_ = 0;
            recent_rate_ = rate;
            if (*recent_rate_ >= delivery_rate_)
                recent_rate_.reset();
        }
        else if (++recent_rate_failures_ >= kRecentRateFailures)
        {
            recent_rate_failures_ = 0;
            delivery_rate_ = *recent_rate_;
            recent_rate_.reset();
        }
    }
    else if (rate > delivery_rate_)
    {
        delivery_rate_ = rate;
        delivery_rate_measured_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
double BandwidthControllerUdp::usableRate() const
{
    return recent_rate_.has_value() ? std::min(delivery_rate_, *recent_rate_) : delivery_rate_;
}

//--------------------------------------------------------------------------------------------------
qint64 BandwidthControllerUdp::standingQueueBytes() const
{
    if (queue_history_size_ < kQueueHistorySize)
        return 0;

    return *std::min_element(queue_history_.begin(), queue_history_.end());
}

//--------------------------------------------------------------------------------------------------
qint64 BandwidthControllerUdp::calculateBandwidth() const
{
    if (!delivery_rate_measured_)
        return std::llround(delivery_rate_);

    const double share = usableRate() >= kMaxBandwidth ? 1 : kBandwidthShare;
    const double target = share * usableRate() - standingQueueBytes() / kQueueDrainTime;

    return std::clamp<qint64>(std::llround(target), kMinBandwidth, kMaxBandwidth);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::setBandwidth(qint64 bandwidth)
{
    if (bandwidth / 1024 != bandwidth_ / 1024)
        LOG(INFO) << "Bandwidth:" << (bandwidth_ / 1024) << "->" << (bandwidth / 1024) << "kB/s";

    bandwidth_ = bandwidth;
}
