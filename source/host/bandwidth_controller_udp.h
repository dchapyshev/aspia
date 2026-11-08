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

#ifndef HOST_BANDWIDTH_CONTROLLER_UDP_H
#define HOST_BANDWIDTH_CONTROLLER_UDP_H

#include <array>
#include <deque>
#include <optional>

#include "host/bandwidth_controller.h"

// Estimates the bandwidth of a session from the bytes acknowledged by the peer.
class BandwidthControllerUdp final : public BandwidthController
{
public:
    // The counters of the channel, all of them grow only.
    struct Counters
    {
        qint64 sent = 0;        // Handed to the channel.
        qint64 transmitted = 0; // Went to the network for the first time.
        qint64 delivered = 0;   // Acknowledged by the peer.
        qint64 in_flight = 0;
        MilliSeconds round_trip{ 0 };
    };

    explicit BandwidthControllerUdp(qint64 bandwidth);
    ~BandwidthControllerUdp() final;

    // BandwidthController implementation.
    void start() final;
    bool startProbe(TimePoint now) final;
    void finishProbe(TimePoint now) final;
    void onBytesSent(qint64 bytes) final;
    qint64 paddingDue(TimePoint now) const final;
    void tick(TimePoint now) final;
    qint64 bandwidth() const final { return bandwidth_; }
    proto::desktop::Overflow::State overflowState(TimePoint now) const final;

    // Has to be called every kPaddingSlice before the padding is asked for.
    void onCounters(TimePoint now, const Counters& counters);

    // Bytes in flight that fill the path at the estimate. |path_delay| is the round trip of the
    // path until the counters have measured it.
    qint64 windowSize(MilliSeconds path_delay) const;

private:
    struct Snapshot
    {
        TimePoint time;
        qint64 sent = 0;
        qint64 transmitted = 0;
        qint64 delivered = 0;
        qint64 in_flight = 0;
        bool loaded = false;
        bool busy = false;
    };

    static constexpr int kSnapshotHistorySize = 100;
    static constexpr int kPeriodSlices = 10;
    static constexpr int kQueueHistorySize = 5;

    void addSnapshot(TimePoint now, const Counters& counters);
    const Snapshot& snapshot(int index) const;
    void updateRoundTrip(TimePoint now, MilliSeconds round_trip);
    void sampleRate(TimePoint now);
    void addSample(const Snapshot& from, const Snapshot& to, double floor);
    void runProbe(TimePoint now, const Counters& counters, qint64 data_bytes);
    void finishProbe(TimePoint now, bool raised);
    qint64 freeWindow(const Counters& counters) const;
    qint64 standingQueueBytes() const;
    qint64 calculateBandwidth() const;
    void setBandwidth(qint64 bandwidth);

    // The highest rate the path delivered over the window.
    double capacity_ = 0;
    bool capacity_measured_ = false;
    std::deque<double> rate_samples_;

    // The busy period under way, by the snapshot it began at and the one it was last sampled at.
    std::optional<int> period_start_;
    int period_sampled_ = 0;

    // The bytes of the frames over the last slices, a probe meets them with care.
    std::array<qint64, kPeriodSlices> data_history_{};
    int data_history_size_ = 0;

    std::array<Snapshot, kSnapshotHistorySize> snapshots_;
    int snapshot_count_ = 0;

    // The queue of the session in bytes over the last slices.
    std::array<qint64, kQueueHistorySize> queue_history_{};
    int queue_history_size_ = 0;

    // The lowest round trip of the current and of the previous period.
    std::optional<MilliSeconds> min_round_trip_;
    MilliSeconds current_min_round_trip_{ 0 };
    MilliSeconds previous_min_round_trip_{ 0 };
    std::optional<TimePoint> min_round_trip_time_;

    // The probe of a higher rate.
    double probe_rate_ = 0;
    double probe_base_ = 0;
    bool probe_growing_ = true;
    int probe_slices_ = 0;
    int probe_slices_left_ = 0;
    bool probe_queued_ = false;
    double probed_rate_ = 0;
    double probe_resolution_ = 1;
    std::optional<TimePoint> probe_start_time_;
    qint64 probe_bytes_sent_ = 0;
    std::optional<qint64> probe_end_byte_;
    std::optional<TimePoint> probe_end_time_;
    std::optional<TimePoint> next_probe_time_;
    Seconds probe_interval_{ 0 };

    qint64 slice_bytes_ = 0;
    qint64 padding_due_ = 0;
    std::optional<TimePoint> last_delay_probe_time_;
    qint64 bandwidth_ = 0;

    Q_DISABLE_COPY_MOVE(BandwidthControllerUdp)
};

#endif // HOST_BANDWIDTH_CONTROLLER_UDP_H
