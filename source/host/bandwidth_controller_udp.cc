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
// rest. The queue is bounded by the window and by the agent, not by the publication.
const double kBandwidthShare = 1.0 / 1.15;

// A delay of the path up to this much is not a queue. The estimate is the highest of this many
// periods, one delay does not make two of them low.
const MilliSeconds kDelayTolerance{ 200 };
const int kRateSamples = 3;

// The excess at which the agent lowers the frame rate.
const MilliSeconds kWarningQueue{ 300 };
const MilliSeconds kCriticalQueue{ 1000 };

// A publication down does not wait for the tick when it is this far under the published bandwidth.
const double kFastDecreaseShare = 0.9;

// The session sends a delay probe this often, only for the acknowledgement it brings after a frame.
const MilliSeconds kDelayProbeInterval{ 950 };

// A window holds two round trips of the rate, or one and the delay tolerance.
const qint64 kMaxWindow = 512 * 1024;

// The lowest round trip is forgotten by twice this time. It outlasts the drain after a narrowing,
// or that queue is kept as the path.
const Seconds kRoundTripPeriod{ 30 };

// A busy period shorter than this is not measured, its ends are known to a slice.
const int kMinPeriodSlices = 5;

// A probe pads the slices to the estimate times the gain. While the rate grows the probe is short,
// a short period is only measured low. A probe that holds pays its queue with the frames under
// video, so it is small, and on a still screen it is long enough to empty a bucket of the path.
const double kGrowingGain = 1.5;
const int kGrowingProbeSlices = 5;
const double kVideoGain = 1.25;
const int kVideoProbeSlices = 5;
const double kIdleGain = 1.25;
const int kIdleProbeSlices = 20;
const double kVideoShare = 0.25;

// When the next probe is made, and how long a probe waits for its bytes.
const Seconds kFirstProbeDelay{ 1 };
const Seconds kProbeInterval{ 10 };
const Seconds kMaxProbeInterval{ 60 };
const Seconds kProbeTimeout{ 10 };

//--------------------------------------------------------------------------------------------------
double toSeconds(TimePoint::duration duration)
{
    return std::chrono::duration<double>(duration).count();
}

} // namespace

//--------------------------------------------------------------------------------------------------
BandwidthControllerUdp::BandwidthControllerUdp(qint64 bandwidth)
    : capacity_(static_cast<double>(bandwidth)),
      probe_interval_(kProbeInterval),
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
    if (last_delay_probe_time_.has_value() && now - *last_delay_probe_time_ < kDelayProbeInterval)
        return false;

    last_delay_probe_time_ = now;
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
    const double seconds = standingQueueBytes() / std::max(capacity_, 1.0);

    if (seconds >= toSeconds(kCriticalQueue))
        return proto::desktop::Overflow::STATE_CRITICAL;
    else if (seconds >= toSeconds(kWarningQueue))
        return proto::desktop::Overflow::STATE_WARNING;

    return proto::desktop::Overflow::STATE_NONE;
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::onCounters(TimePoint now, const Counters& counters)
{
    const qint64 slice_bytes = slice_bytes_;
    data_history_[data_history_size_++ % kPeriodSlices] = slice_bytes_ - padding_due_;
    slice_bytes_ = 0;
    padding_due_ = 0;

    updateRoundTrip(now, counters.round_trip);
    addSnapshot(now, counters);

    // What is in flight above a round trip of the estimate stands in the path.
    const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
    const double in_path = std::max(static_cast<double>(counters.in_flight) - capacity_ * round_trip, 0.0);
    const qint64 backlog = counters.sent - counters.transmitted;

    queue_history_[queue_history_size_ % kQueueHistorySize] = backlog + std::llround(in_path);
    ++queue_history_size_;

    sampleRate(now);
    runProbe(now, counters, slice_bytes);

    const qint64 target = calculateBandwidth();
    if (target < bandwidth_ * kFastDecreaseShare)
        setBandwidth(target);
}

//--------------------------------------------------------------------------------------------------
qint64 BandwidthControllerUdp::windowSize(MilliSeconds path_delay) const
{
    const double round_trip = toSeconds(min_round_trip_.value_or(path_delay));
    const double seconds = std::max(2 * round_trip, round_trip + toSeconds(kDelayTolerance));
    const double top = std::max(capacity_, probe_rate_);

    return std::min(std::llround(top * seconds), kMaxWindow);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::addSnapshot(TimePoint now, const Counters& counters)
{
    Snapshot& snapshot = snapshots_[snapshot_count_ % kSnapshotHistorySize];

    snapshot.time = now;
    snapshot.sent = counters.sent;
    snapshot.transmitted = counters.transmitted;
    snapshot.delivered = counters.delivered;
    snapshot.in_flight = counters.in_flight;

    // Loaded while a probe pads, when the host queued or when the estimate's worth of bytes left in
    // the interval and over the last slices. Busy also while a round trip and a slice of the estimate
    // were in flight, which only keeps a period going, a delay alone holds bytes in flight as well.
    snapshot.loaded = false;
    snapshot.busy = false;
    if (snapshot_count_ > 0)
    {
        const Snapshot& previous = this->snapshot(snapshot_count_ - 1);
        const Snapshot& earlier = this->snapshot(std::max(snapshot_count_ - kMinPeriodSlices, 0));
        const double left = static_cast<double>(counters.transmitted - previous.transmitted);
        const double left_lately = static_cast<double>(counters.transmitted - earlier.transmitted);
        const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
        const bool probing = probe_slices_left_ > 0 ||
                             (probe_end_byte_.has_value() && counters.delivered < *probe_end_byte_);
        snapshot.loaded = probing || counters.sent > counters.transmitted ||
                          previous.sent > previous.transmitted ||
                          (left >= capacity_ * toSeconds(now - previous.time) &&
                           left_lately >= capacity_ * toSeconds(now - earlier.time));
        snapshot.busy = snapshot.loaded || static_cast<double>(previous.in_flight) >
                                           capacity_ * (round_trip + toSeconds(kPaddingSlice));
    }

    ++snapshot_count_;
}

//--------------------------------------------------------------------------------------------------
const BandwidthControllerUdp::Snapshot& BandwidthControllerUdp::snapshot(int index) const
{
    return snapshots_[index % kSnapshotHistorySize];
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
// The bytes of a whole busy period over its time are the rate of the path, and the ends known to a
// slice only lower that. The highest of the last samples is the estimate, a delay only lowers
// samples too, and the estimate holds while nothing measures the path.
void BandwidthControllerUdp::sampleRate(TimePoint /* now */)
{
    const int index = snapshot_count_ - 1;
    if (index < 1)
        return;

    const Snapshot& last = snapshot(index);
    if (last.busy && (period_start_.has_value() || last.loaded))
    {
        if (!period_start_.has_value())
        {
            period_start_ = index - 1;
            period_sampled_ = index - 1;
        }
        else if (index - period_sampled_ >= kPeriodSlices &&
                 index - *period_start_ >= 2 * kPeriodSlices)
        {
            addSample(snapshot(period_sampled_), last, 0);
            period_sampled_ = index;
        }
    }
    else if (period_start_.has_value())
    {
        // A period that ends a round trip and a slice after the padding of a probe kept its pace,
        // so the rate the probe left at is the rate of the period, its end known to a slice only
        // lowers it.
        double floor = 0;
        const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));
        if (probe_end_byte_.has_value() && probe_end_time_.has_value() &&
            snapshot(index - 1).delivered >= *probe_end_byte_ &&
            toSeconds(snapshot(index - 1).time - *probe_end_time_) <=
                round_trip + toSeconds(kPaddingSlice))
        {
            const double seconds = toSeconds(*probe_end_time_ - *probe_start_time_ + kPaddingSlice);
            floor = std::min(probe_bytes_sent_ / seconds, probe_rate_);
        }
        if (index - 1 - period_sampled_ >= kMinPeriodSlices)
            addSample(snapshot(period_sampled_), snapshot(index - 1), floor);
        period_start_.reset();
    }

    if (!rate_samples_.empty())
    {
        const double rate = *std::max_element(rate_samples_.begin(), rate_samples_.end());
        capacity_ = std::clamp<double>(rate, kMinBandwidth, kMaxBandwidth);
        capacity_measured_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::addSample(const Snapshot& from, const Snapshot& to, double floor)
{
    const double seconds = toSeconds(to.time - from.time);
    const qint64 bytes = to.delivered - from.delivered;
    if (seconds <= 0 || bytes <= 0)
        return;

    const double rate = std::max(bytes / seconds, floor);
    if (rate_samples_.size() == kRateSamples)
        rate_samples_.pop_front();
    rate_samples_.push_back(rate);
}

//--------------------------------------------------------------------------------------------------
// The padding tops up what left since the probe began to its rate, whatever the timer did.
void BandwidthControllerUdp::runProbe(TimePoint now, const Counters& counters, qint64 slice_bytes)
{
    const bool queued = counters.sent > counters.transmitted || standingQueueBytes() > 0;

    if (probe_slices_left_ > 0)
    {
        probe_bytes_sent_ += slice_bytes;
        probe_queued_ = probe_queued_ || standingQueueBytes() > 0;

        const double elapsed = toSeconds(now - *probe_start_time_) + toSeconds(kPaddingSlice);
        padding_due_ = std::max<qint64>(std::llround(probe_rate_ * elapsed) - probe_bytes_sent_, 0);
        padding_due_ = std::min(padding_due_, freeWindow(counters));
        --probe_slices_left_;

        if (probe_slices_left_ == 0)
        {
            probe_bytes_sent_ += padding_due_;
            probe_end_byte_ = counters.sent + padding_due_;
            probe_end_time_ = now;
        }
        return;
    }

    if (probe_end_byte_.has_value())
    {
        probe_queued_ = probe_queued_ || standingQueueBytes() > 0;

        // The probe is measured by the busy period it made, so it is over when that period is.
        const bool delivered = counters.delivered >= *probe_end_byte_ && !queued &&
                               !period_start_.has_value();
        if (!delivered && now - *probe_end_time_ < kProbeTimeout)
            return;

        // A probe at the ceiling that left no queue was taken by the path.
        if (delivered && !probe_queued_ && probe_rate_ >= kMaxBandwidth)
        {
            rate_samples_.assign(kRateSamples, kMaxBandwidth);
            capacity_ = kMaxBandwidth;
            capacity_measured_ = true;
        }

        // A period is measured within a slice of its time at either end, a rise under that is not
        // one.
        probe_resolution_ = 1.0 + 1.0 / (probe_slices_ + 1);
        finishProbe(now, delivered && capacity_ > probe_base_ * probe_resolution_);
    }

    if (!next_probe_time_.has_value())
        next_probe_time_ = now + kFirstProbeDelay;

    // A probe that found no room proved it for the estimate of its time, not for a lower one.
    if (capacity_ * kVideoGain < probed_rate_)
    {
        probed_rate_ = 0;
        next_probe_time_ = now;
        probe_interval_ = kProbeInterval;
    }

    // A probe over a queue measures the queue.
    if (now < *next_probe_time_ || queued || capacity_ >= kMaxBandwidth)
        return;

    const double round_trip = toSeconds(min_round_trip_.value_or(MilliSeconds::zero()));

    probe_base_ = capacity_;
    qint64 data_bytes = 0;
    for (qint64 bytes : data_history_)
        data_bytes += bytes;
    const bool video = static_cast<double>(data_bytes) >=
                       kVideoShare * static_cast<double>(bandwidth_) * kPeriodSlices * toSeconds(kPaddingSlice);

    const double gain = probe_growing_ ? kGrowingGain : video ? kVideoGain : kIdleGain;
    const int slices = probe_growing_ ? kGrowingProbeSlices :
                       video ? kVideoProbeSlices : kIdleProbeSlices;

    probe_rate_ = std::min<double>(capacity_ * gain, kMaxBandwidth);
    probe_slices_ = slices + static_cast<int>(std::ceil(round_trip / toSeconds(kPaddingSlice)));
    probe_slices_left_ = probe_slices_;
    probe_start_time_ = now;
    probe_bytes_sent_ = 0;
    probe_queued_ = false;

    padding_due_ = std::min(std::llround(probe_rate_ * toSeconds(kPaddingSlice)), freeWindow(counters));
    --probe_slices_left_;
}

//--------------------------------------------------------------------------------------------------
// The padding of a probe stops at the window, past it the probe would only queue on the host.
qint64 BandwidthControllerUdp::freeWindow(const Counters& counters) const
{
    const qint64 window = windowSize(min_round_trip_.value_or(MilliSeconds::zero()));
    return std::max<qint64>(window - counters.in_flight - (counters.sent - counters.transmitted), 0);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::finishProbe(TimePoint now, bool raised)
{
    const double probe_rate = probe_rate_;
    probe_rate_ = 0;
    probe_end_byte_.reset();
    probe_end_time_.reset();
    probe_start_time_.reset();

    probe_growing_ = raised;
    if (raised)
    {
        next_probe_time_ = now;
        probe_interval_ = kProbeInterval;
    }
    else
    {
        probed_rate_ = std::max(probe_rate / probe_resolution_, probed_rate_);
        next_probe_time_ = now + probe_interval_;
        probe_interval_ = std::min(probe_interval_ * 2, kMaxProbeInterval);
    }
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
    if (!capacity_measured_)
        return std::llround(capacity_);

    const double share = capacity_ >= kMaxBandwidth ? 1 : kBandwidthShare;

    return std::clamp<qint64>(std::llround(share * capacity_), kMinBandwidth, kMaxBandwidth);
}

//--------------------------------------------------------------------------------------------------
void BandwidthControllerUdp::setBandwidth(qint64 bandwidth)
{
    if (bandwidth / 1024 != bandwidth_ / 1024)
        LOG(INFO) << "Bandwidth:" << (bandwidth_ / 1024) << "->" << (bandwidth / 1024) << "kB/s";

    bandwidth_ = bandwidth;
}
