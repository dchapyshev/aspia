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

#ifndef HOST_BANDWIDTH_CONTROLLER_TCP_H
#define HOST_BANDWIDTH_CONTROLLER_TCP_H

#include <optional>

#include "host/bandwidth_controller.h"

// Estimates the bandwidth of a session from the delay of the probes that wait in the same queue as
// the session traffic. A higher rate is tried by padding the traffic up to it.
class BandwidthControllerTcp final : public BandwidthController
{
public:
    BandwidthControllerTcp();
    ~BandwidthControllerTcp() final;

    // BandwidthController implementation.
    void start() final;
    bool startProbe(TimePoint now) final;
    void finishProbe(TimePoint now) final;
    void onBytesSent(qint64 bytes) final;
    qint64 paddingDue(TimePoint now) const final;
    void tick(TimePoint now) final;
    qint64 bandwidth() const final { return bandwidth_; }
    proto::desktop::Overflow::State overflowState(TimePoint now) const final;

private:
    void startTest(TimePoint now);
    void measureCapacity(double capacity, MilliSeconds queue, TimePoint now);
    void drainQueue(MilliSeconds queue, TimePoint now);
    void trustProbe(TimePoint send_time, MilliSeconds queue);
    void finishTest(double interval, bool steady);
    void setBandwidth(double bandwidth);

    qint64 bandwidth_ = 0;

    // The rate the path has to carry until the next probe to become the bandwidth, zero between
    // the tests, and the one it had to carry until the probe in flight.
    qint64 test_rate_ = 0;
    qint64 probe_rate_ = 0;

    // The rate of the path, measured from a growing queue or passed without one. Zero until a queue
    // was seen. The time is that of the last measurement, the interval is the one to the next test
    // over the capacity.
    qint64 capacity_ = 0;
    std::optional<TimePoint> capacity_time_;
    Seconds capacity_test_interval_{ 0 };

    // The queue of the model in front of the last probe, and the one in front of the probe in
    // flight when the path passes the capacity.
    MilliSeconds queue_{ 0 };
    MilliSeconds expected_excess_{ 0 };

    // The last probe the model trusts, the queue in front of it and the bytes sent since. The probe
    // after it showed more than the model foresaw.
    std::optional<TimePoint> trusted_time_;
    MilliSeconds trusted_queue_{ 0 };
    qint64 trusted_bytes_ = 0;
    bool suspect_ = false;

    // The delay of the path, and the delay of the last probe over it.
    std::optional<MilliSeconds> base_delay_;
    MilliSeconds last_excess_{ 0 };

    // The probe in flight, the one before it and the bytes sent between them.
    std::optional<TimePoint> probe_send_time_;
    std::optional<TimePoint> previous_probe_time_;
    qint64 probe_bytes_ = 0;
    qint64 sent_bytes_ = 0;

    Q_DISABLE_COPY_MOVE(BandwidthControllerTcp)
};

#endif // HOST_BANDWIDTH_CONTROLLER_TCP_H
