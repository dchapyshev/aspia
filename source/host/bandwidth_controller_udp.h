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

    // Bytes in flight that fill the path at the highest recent rate. |path_delay| is the round trip
    // of the path until the counters have measured it.
    qint64 windowSize(MilliSeconds path_delay) const;

private:
    struct Sample
    {
        TimePoint time;
        qint64 sent = 0;
        qint64 transmitted = 0;
        qint64 delivered = 0;
    };

    static constexpr int kCountersHistorySize = 42;
    static constexpr int kQueueHistorySize = 5;

    void addCounters(TimePoint now, const Counters& counters);
    const Sample& countersAt(int slices_ago) const;
    void updateRoundTrip(TimePoint now, MilliSeconds round_trip);
    void updateDeliveryRate();
    void addRateSample(double rate, double seconds);
    void updateRecentRate();
    void updatePeakRate(TimePoint now);
    void runRateTest(TimePoint now, qint64 data_bytes);
    void finishRateTest(TimePoint now);
    void startRateTest(TimePoint now);
    void applyRateTestResult(double rate, bool passed);
    double usableRate() const;
    qint64 standingQueueBytes() const;
    qint64 calculateBandwidth() const;
    void setBandwidth(qint64 bandwidth);

    // The rate delivered over the busy time, and that time. It is not known until the first busy
    // slice. The peak is the highest one of the last minute.
    double delivery_rate_ = 0;
    double busy_time_ = 0;
    bool delivery_rate_measured_ = false;
    double peak_rate_ = 0;
    std::optional<TimePoint> peak_rate_time_;

    // The last byte that went out of a busy slice, its delivery still counts as busy.
    qint64 last_busy_byte_ = 0;

    // The rate of the last seconds of a standing queue when it is far under the estimate.
    std::optional<double> recent_rate_;
    int queued_slices_ = 0;

    // The queue of the session in bytes over the last slices.
    std::array<qint64, kQueueHistorySize> queue_history_{};
    int queue_history_size_ = 0;

    std::array<Sample, kCountersHistorySize> counters_history_;
    int counters_history_size_ = 0;

    // The lowest round trip of the current and of the previous period.
    std::optional<MilliSeconds> min_round_trip_;
    MilliSeconds current_min_round_trip_{ 0 };
    MilliSeconds previous_min_round_trip_{ 0 };
    std::optional<TimePoint> min_round_trip_time_;

    // The test of a higher rate.
    double test_rate_ = 0;
    double test_base_rate_ = 0;
    double test_gain_ = 0;
    int test_slices_left_ = 0;
    int recent_rate_failures_ = 0;
    bool test_retried_ = false;
    std::optional<TimePoint> test_measure_start_;
    std::optional<TimePoint> test_result_time_;
    std::optional<TimePoint> next_test_time_;
    Seconds test_interval_{ 0 };

    qint64 slice_bytes_ = 0;
    qint64 padding_due_ = 0;
    std::optional<TimePoint> last_probe_time_;
    qint64 bandwidth_ = 0;

    Q_DISABLE_COPY_MOVE(BandwidthControllerUdp)
};

#endif // HOST_BANDWIDTH_CONTROLLER_UDP_H
