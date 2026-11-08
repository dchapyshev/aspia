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

// The regulator on a modelled ENet path: fragments, a window, retransmissions by the timeout and by
// the later acknowledgements, a drop tail queue at the bottleneck, a delay of the path after it, and
// an encoder under the published bandwidth that holds its frames while the host has a backlog.

#include "host/bandwidth_controller_udp.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

const qint64 kFragment = 1172;
const qint64 kWireOverhead = 60;
const qint64 kMinBandwidth = 8 * 1024;
const qint64 kMaxBandwidth = 4800 * 1024;

// Of the regulator, the budgets below follow from them.
const qint64 kMaxWindow = 512 * 1024;
const double kPublishedShare = 1 / 1.15;
const double kBacklogBudget = 0.25; // Seconds of the publication the agent lets the host hold.
const qint64 kStartBandwidth = 128 * 1024;
const qint64 kKeyFrame = 497 * 1024;
const qint64 kStartKeyFrame = 290 * 1024;
const qint64 kProbeBytes = 40;
const int kFrameRate = 20;

// The delay of the echo over the path of the 90 seconds of the measurement.
const int kMeasuredNoise[] =
{
    5, 2, 2, 47, 37, 156, 108, 5, 2, 6, 3, 6, 1, 2, 2, 7, 4, 123, 59, 36, 201, 61, 4, 4, 1, 3, 3, 4,
    2, 78, 1, 3, 4, 3, 2, 4, 3, 8, 3, 2, 2, 6, 4, 34, 179, 145, 1, 9, 2, 2, 2, 21, 33, 2, 1, 2, 95,
    2, 7, 1, 5, 4, 1, 3, 19, 0, 5, 3, 131, 74, 4, 2, 19, 1, 5, 5, 2, 2, 3, 4, 39, 68, 90, 0, 7, 5, 5,
    22, 26, 2
};

// The same numbers from the same seed on every platform.
class Random
{
public:
    explicit Random(quint32 seed) : engine_(seed) {}

    // In [0, 1).
    double uniform() { return static_cast<double>(engine_() & 0xFFFFFFFF) / 4294967296.0; }

    int integer(int min, int max)
    {
        return min + static_cast<int>(uniform() * (max - min + 1));
    }

    bool chance(double probability) { return uniform() < probability; }

    // With the mean of 1.
    double lognormal(double cv)
    {
        const double sigma = std::sqrt(std::log(1 + cv * cv));
        const double radius = std::sqrt(-2 * std::log(1 - uniform()));
        const double normal = radius * std::cos(2 * kPi * uniform());
        return std::exp(sigma * normal - sigma * sigma / 2);
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    std::mt19937 engine_;
};

// The measured path of the session, in bytes of the network and of the application.
const qint64 kCapacity = 1270 * 1024;
const qint64 kCapacityApp = kCapacity * kFragment / (kFragment + kWireOverhead);

struct Path
{
    qint64 capacity = kCapacity;
    int delay_ms = 13;          // One way.
    int queue_ms = 250;         // Of the bottleneck at the initial capacity.
    double loss = 0;
    int noise_scale = 0;
    int noise_phase = 0;
    int wake_ms = 0;            // The way back is this late after it was silent for 100 ms.
    int timer_jitter_ms = 10;   // The slice timer fires this much earlier or later.
    quint32 seed = 1;
};

enum class Content { IDLE, VIDEO };

class Stand
{
public:
    explicit Stand(const Path& path, Content content = Content::VIDEO, double fill = 0.7)
        : path_(path),
          content_(content),
          fill_(fill),
          random_(path.seed),
          timer_random_(path.seed + 1000),
          controller_(kStartBandwidth),
          queue_bytes_(path.capacity * path.queue_ms / 1000),
          published_(controller_.bandwidth())
    {
        // Nothing
    }

    void run(Seconds duration)
    {
        const qint64 end = ms_ + duration.count() * 1000;
        while (ms_ < end)
            step();
    }

    void setCapacity(qint64 capacity) { path_.capacity = capacity; }
    void setContent(Content content, double fill = 0.7) { content_ = content; fill_ = fill; }
    void setNoise(int scale) { path_.noise_scale = scale; }
    void keyFrame() { key_frame_ = true; }
    void outage(Seconds duration) { outage_end_ = ms_ + duration.count() * 1000; }

    qint64 bandwidth() const { return published_; }
    qint64 minBandwidth() const { return min_bandwidth_; }
    qint64 maxBandwidth() const { return max_bandwidth_; }

    // The share of the seconds since the metrics were reset.
    double shareBelow(qint64 bandwidth) const
    {
        int count = 0;
        for (qint64 value : seconds_)
            count += value < bandwidth ? 1 : 0;
        return seconds_.empty() ? 0 : static_cast<double>(count) / seconds_.size();
    }

    qint64 meanBandwidth() const
    {
        qint64 sum = 0;
        for (qint64 value : seconds_)
            sum += value;
        return seconds_.empty() ? 0 : sum / static_cast<qint64>(seconds_.size());
    }

    MilliSeconds frameDelay(double share) const
    {
        if (frame_delays_.empty())
            return MilliSeconds::zero();

        std::vector<qint64> sorted = frame_delays_;
        std::sort(sorted.begin(), sorted.end());
        return MilliSeconds(sorted[static_cast<size_t>(share * (sorted.size() - 1))]);
    }

    void resetMetrics()
    {
        min_bandwidth_ = published_;
        max_bandwidth_ = published_;
        seconds_.clear();
        frame_delays_.clear();
        trace_.clear();
    }

    std::string trace() const { return trace_; }

private:
    struct Command
    {
        qint64 size = 0;
        qint64 sent_ms = 0;
        qint64 timeout_ms = 0;
        int frame = -1;
        int attempts = 0;
        bool received = false;
        bool acknowledged = false;
    };

    struct Packet
    {
        double time;
        int command;
        qint64 echo_ms;
    };

    struct Frame
    {
        qint64 queued_ms;
        int left;
    };

    void step()
    {
        ++ms_;

        receive();
        flushAcknowledgements();
        acknowledge();
        encode();

        if (ms_ >= next_slice_ms_)
        {
            slice();

            // A repeating timer keeps its schedule, only each timeout moves around it.
            const int jitter = path_.timer_jitter_ms;
            slice_index_ += 1;
            next_slice_ms_ = (slice_index_ + 1) * 100 + timer_random_.integer(-jitter, jitter);
        }
        if (ms_ % 1000 == 0)
            tick();

        service();
    }

    void slice()
    {
        const qint64 round_trip = std::min(last_round_trip_, lowest_round_trip_);

        BandwidthControllerUdp::Counters counters;
        counters.sent = sent_;
        counters.transmitted = transmitted_;
        counters.delivered = delivered_;
        counters.in_flight = in_flight_;
        counters.round_trip = MilliSeconds(round_trip);

        const TimePoint now = TimePoint() + MilliSeconds(ms_);
        controller_.onCounters(now, counters);
        published_ = controller_.bandwidth();

        // The session sets the window on every slice, before the padding of a test leaves.
        window_ = controller_.windowSize(MilliSeconds(round_trip_));

        if (controller_.startProbe(now))
        {
            send(kProbeBytes, -1);
            send(kProbeBytes, -1);
        }

        const qint64 due = controller_.paddingDue(now);
        if (due > 0)
            send(due, -1);
    }

    void tick()
    {
        controller_.tick(TimePoint() + MilliSeconds(ms_));
        published_ = controller_.bandwidth();
        window_ = controller_.windowSize(MilliSeconds(round_trip_));

        min_bandwidth_ = std::min(min_bandwidth_, published_);
        max_bandwidth_ = std::max(max_bandwidth_, published_);
        seconds_.push_back(published_);

        trace_ += std::to_string(ms_ / 1000) + "s pub=" + std::to_string(published_ / 1024) +
            " win=" + std::to_string(window_ / 1024) + " q=" +
            std::to_string((sent_ - transmitted_) / 1024) + " flight=" +
            std::to_string(in_flight_ / 1024) + " rtt=" + std::to_string(round_trip_) + "\n";
    }

    void encode()
    {
        // Not in step with the slices, so that a slice does not always find a frame in flight.
        if ((ms_ + 17) % (1000 / kFrameRate) != 0)
            return;

        if (!first_frame_)
        {
            first_frame_ = true;
            if (content_ == Content::VIDEO)
                sendFrame(kStartKeyFrame);
            return;
        }

        if (static_cast<double>(sent_ - transmitted_) > static_cast<double>(published_) * kBacklogBudget)
            return;

        if (key_frame_)
        {
            key_frame_ = false;
            sendFrame(kKeyFrame);
            return;
        }

        if (content_ == Content::IDLE)
        {
            if (ms_ % 1000 == 0)
                sendFrame(300);
            return;
        }

        const double frame =
            static_cast<double>(published_) * fill_ / kFrameRate * random_.lognormal(0.5);
        sendFrame(std::max<qint64>(std::llround(frame), 100));
    }

    void sendFrame(qint64 bytes)
    {
        const int fragments = static_cast<int>((bytes + kFragment - 1) / kFragment);
        frames_.push_back({ ms_, fragments });
        send(bytes, frame_base_ + static_cast<int>(frames_.size()) - 1);
    }

    void send(qint64 bytes, int frame)
    {
        controller_.onBytesSent(bytes);
        sent_ += bytes;

        while (bytes > 0)
        {
            Command command;
            command.size = std::min(bytes, kFragment);
            command.frame = frame;
            bytes -= command.size;

            commands_.push_back(command);
            outgoing_.push_back(static_cast<int>(commands_.size()) - 1);
        }

        // UdpChannel flushes the host after every message.
        service();
    }

    void service()
    {
        // The timer of the commands restarts with every acknowledgement.
        if (!in_transit_.empty())
        {
            const Command& head = commands_[in_transit_.begin()->second];
            if (ms_ - std::max(last_receive_ms_, head.sent_ms) >= head.timeout_ms)
            {
                for (auto it = in_transit_.begin(); it != in_transit_.end();)
                {
                    Command& command = commands_[it->second];
                    if (ms_ - std::max(last_receive_ms_, command.sent_ms) < command.timeout_ms)
                    {
                        ++it;
                        continue;
                    }

                    command.timeout_ms *= 2;
                    in_flight_ -= command.size;
                    resend_.push_back(it->second);
                    it = in_transit_.erase(it);
                }
            }
        }

        const qint64 window = std::max<qint64>(window_, kFragment + 28);

        while (true)
        {
            std::deque<int>& queue = !resend_.empty() ? resend_ : outgoing_;
            if (queue.empty())
                break;

            Command& command = commands_[queue.front()];
            if (command.acknowledged)
            {
                queue.pop_front();
                continue;
            }

            if (in_flight_ + command.size > window)
                break;

            const int id = queue.front();
            queue.pop_front();

            if (++command.attempts == 1)
                transmitted_ += command.size;
            if (command.timeout_ms == 0)
                command.timeout_ms = round_trip_ + 4 * round_trip_variance_;

            command.sent_ms = ms_;
            in_flight_ += command.size;
            in_transit_[++send_order_] = id;
            transmit(id);
        }
    }

    void transmit(int id)
    {
        if (ms_ < outage_end_)
            return;

        if (path_.loss > 0 && random_.chance(path_.loss))
            return;

        const double now = ms_ / 1000.0;
        const double capacity = static_cast<double>(path_.capacity);
        const double wire = static_cast<double>(commands_[id].size + kWireOverhead);

        bottleneck_ = std::max(bottleneck_, now);
        if ((bottleneck_ - now) * capacity > queue_bytes_)
            return;

        bottleneck_ += wire / capacity;

        double arrival = bottleneck_ + path_.delay_ms / 1000.0;
        if (path_.noise_scale > 0)
        {
            const int count = static_cast<int>(std::size(kMeasuredNoise));
            const int index = (static_cast<int>(bottleneck_) + path_.noise_phase) % count;
            arrival += kMeasuredNoise[index] * path_.noise_scale / 1000.0;
        }

        last_arrival_ = std::max(last_arrival_, arrival);
        forward_.push_back({ last_arrival_, id, ms_ });
    }

    void receive()
    {
        const double now = ms_ / 1000.0;

        while (!forward_.empty() && forward_.front().time <= now)
        {
            const Packet packet = forward_.front();
            forward_.pop_front();

            pending_acks_.push_back(packet);

            Command& command = commands_[packet.command];
            if (command.received)
                continue;

            command.received = true;
            if (command.frame < 0)
                continue;

            --frames_[command.frame - frame_base_].left;
            while (!frames_.empty() && frames_.front().left == 0)
            {
                frame_delays_.push_back(ms_ - frames_.front().queued_ms);
                frames_.pop_front();
                ++frame_base_;
            }
        }
    }

    void flushAcknowledgements()
    {
        if (ms_ % 2 != 0)
            return;

        if (pending_acks_.empty())
            return;

        double arrival = (ms_ + path_.delay_ms) / 1000.0;
        if (path_.wake_ms > 0 && ms_ - last_backward_ms_ >= 100)
            arrival += path_.wake_ms / 1000.0;
        last_backward_ms_ = ms_;
        last_backward_arrival_ = std::max(last_backward_arrival_, arrival);

        for (const Packet& packet : pending_acks_)
        {
            if (ms_ < outage_end_)
                continue;
            backward_.push_back({ last_backward_arrival_, packet.command, packet.echo_ms });
        }
        pending_acks_.clear();
    }

    void acknowledge()
    {
        const double now = ms_ / 1000.0;

        while (!backward_.empty() && backward_.front().time <= now)
        {
            const Packet packet = backward_.front();
            backward_.pop_front();

            const qint64 sample = std::max<qint64>(ms_ - packet.echo_ms, 1);
            updateRoundTrip(sample);

            Command& command = commands_[packet.command];
            if (command.acknowledged)
                continue;

            command.acknowledged = true;
            delivered_ += command.size;
            last_receive_ms_ = ms_;

            for (auto it = in_transit_.begin(); it != in_transit_.end(); ++it)
            {
                if (it->second != packet.command)
                    continue;

                in_flight_ -= command.size;
                retransmitBefore(it);
                break;
            }
        }
    }

    // A command that went before the acknowledged one by more than the round trip is lost.
    void retransmitBefore(std::map<qint64, int>::iterator acknowledged)
    {
        const qint64 sent_ms = commands_[acknowledged->second].sent_ms;
        const qint64 delay = round_trip_ + round_trip_ / 8;

        for (auto it = in_transit_.begin(); it != acknowledged;)
        {
            Command& command = commands_[it->second];
            if (command.sent_ms >= sent_ms || ms_ - command.sent_ms < delay)
            {
                ++it;
                continue;
            }

            in_flight_ -= command.size;
            resend_.push_back(it->second);
            it = in_transit_.erase(it);
        }

        in_transit_.erase(acknowledged);
    }

    void updateRoundTrip(qint64 sample)
    {
        if (!round_trip_known_)
        {
            round_trip_known_ = true;
            round_trip_ = sample;
            round_trip_variance_ = (sample + 1) / 2;
        }
        else
        {
            round_trip_variance_ -= round_trip_variance_ / 4;
            if (sample >= round_trip_)
            {
                const qint64 difference = sample - round_trip_;
                round_trip_variance_ += difference / 4;
                round_trip_ += difference / 8;
            }
            else
            {
                const qint64 difference = round_trip_ - sample;
                round_trip_variance_ += difference / 4;
                round_trip_ -= difference / 8;
            }
        }

        lowest_round_trip_ = std::min(lowest_round_trip_, round_trip_);

        if (ms_ - round_trip_epoch_ >= 5000)
        {
            last_round_trip_ = lowest_round_trip_;
            lowest_round_trip_ = round_trip_;
            round_trip_epoch_ = ms_;
        }
    }

    Path path_;
    Content content_;
    double fill_;
    Random random_;
    Random timer_random_;
    BandwidthControllerUdp controller_;
    double queue_bytes_;

    qint64 ms_ = 0;
    qint64 next_slice_ms_ = 100;
    qint64 slice_index_ = 0;
    qint64 published_;
    qint64 window_ = 16 * 1024;
    qint64 outage_end_ = 0;
    bool first_frame_ = false;
    bool key_frame_ = false;

    qint64 sent_ = 0;
    qint64 transmitted_ = 0;
    qint64 delivered_ = 0;
    qint64 in_flight_ = 0;

    std::deque<Command> commands_;
    std::deque<int> outgoing_;
    std::deque<int> resend_;
    std::map<qint64, int> in_transit_;
    qint64 send_order_ = 0;

    double bottleneck_ = 0;
    double last_arrival_ = 0;
    std::deque<Packet> forward_;
    std::deque<Packet> backward_;
    std::vector<Packet> pending_acks_;

    std::deque<Frame> frames_;
    int frame_base_ = 0;

    bool round_trip_known_ = false;
    qint64 round_trip_ = 500;
    qint64 round_trip_variance_ = 0;
    qint64 last_round_trip_ = 500;
    qint64 lowest_round_trip_ = 500;
    qint64 round_trip_epoch_ = 0;
    qint64 last_receive_ms_ = 0;
    qint64 last_backward_ms_ = 0;
    double last_backward_arrival_ = 0;

    qint64 min_bandwidth_ = kStartBandwidth;
    qint64 max_bandwidth_ = kStartBandwidth;
    std::vector<qint64> seconds_;
    std::vector<qint64> frame_delays_;
    std::string trace_;
};

// Every scenario runs with a number of seeds.
const quint32 kSeeds = 30;

class BandwidthControllerUdpStandTest : public testing::TestWithParam<quint32>
{
protected:
    Path seededPath() const
    {
        Path path;
        path.seed = GetParam();
        return path;
    }
};

} // namespace

INSTANTIATE_TEST_SUITE_P(Seeds, BandwidthControllerUdpStandTest, testing::Range(1u, kSeeds + 1));

//--------------------------------------------------------------------------------------------------
TEST(BandwidthControllerUdpTest, ProbeIsSentEverySecond)
{
    BandwidthControllerUdp controller(kStartBandwidth);
    const TimePoint start = TimePoint() + Hours(1);

    EXPECT_TRUE(controller.startProbe(start));
    EXPECT_FALSE(controller.startProbe(start + MilliSeconds(100)));
    EXPECT_TRUE(controller.startProbe(start + MilliSeconds(1000)));
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, StartFindsTheCapacity)
{
    Stand stand(seededPath());
    stand.run(Seconds(30));

    stand.resetMetrics();
    stand.run(Seconds(5));
    EXPECT_GE(stand.meanBandwidth(), std::llround(kCapacityApp * kPublishedShare * 6 / 7)) << stand.trace();

    // The publication follows the estimate alone, a sample of ten slices is within one of them.
    stand.resetMetrics();
    stand.run(Seconds(120));
    const double floor = kPublishedShare * 10 / 11;
    EXPECT_GE(stand.minBandwidth(), std::llround(kCapacityApp * floor)) << stand.trace();
    EXPECT_LE(stand.maxBandwidth(), kCapacityApp * 12 / 10) << stand.trace();
    EXPECT_LE(stand.frameDelay(0.95), MilliSeconds(500)) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
// Nothing but the tests of the regulator carries data, and the way back may sleep while idle.
TEST_P(BandwidthControllerUdpStandTest, StartWhileIdleFindsTheCapacity)
{
    const qint64 capacities[] = { 100 * 1024, 300 * 1024, kCapacity, 5 * 1024 * 1024 };

    for (qint64 capacity : capacities)
    {
        for (int wake_ms : { 0, 50 })
        {
            SCOPED_TRACE(testing::Message() << "capacity " << capacity / 1024 << " wake "
                                            << wake_ms);

            Path path = seededPath();
            path.capacity = capacity;
            path.wake_ms = wake_ms;

            Stand stand(path, Content::IDLE);
            stand.run(Seconds(10));
            stand.resetMetrics();
            stand.run(Seconds(3));

            // A period of six intervals measures the path within a slice of its time.
            const qint64 capacity_app = capacity * kFragment / (kFragment + kWireOverhead);
            const double floor = kPublishedShare * 6 / 7;
            EXPECT_GE(stand.meanBandwidth(), std::llround(capacity_app * floor)) << stand.trace();
            EXPECT_LE(stand.maxBandwidth(), capacity_app * 12 / 10) << stand.trace();
        }
    }
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, StartWhileIdleOnLanReachesTheCeiling)
{
    Path path = seededPath();
    path.capacity = 100 * 1024 * 1024;
    path.delay_ms = 1;

    Stand stand(path, Content::IDLE);
    stand.run(Seconds(15));
    EXPECT_GE(stand.bandwidth(), kMaxBandwidth * 9 / 10) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, LanReachesTheCeiling)
{
    Path path = seededPath();
    path.capacity = 100 * 1024 * 1024;
    path.delay_ms = 1;

    Stand stand(path);
    stand.run(Seconds(60));
    EXPECT_EQ(stand.bandwidth(), kMaxBandwidth) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, NoiseOfTheDelayWhileIdle)
{
    for (int scale : { 1, 2 })
    {
        SCOPED_TRACE(testing::Message() << "scale " << scale);

        Stand stand(seededPath());
        stand.run(Seconds(60));
        const qint64 before = stand.bandwidth();

        stand.setContent(Content::IDLE);
        stand.setNoise(scale);
        stand.run(Seconds(10));
        stand.resetMetrics();
        stand.run(Seconds(600));

        // A delay does not move the publication, only a probe that measures less than the estimate.
        EXPECT_EQ(stand.shareBelow(before * 7 / 10), 0) << stand.trace();
        EXPECT_GT(stand.minBandwidth(), before * 8 / 10) << stand.trace();
        EXPECT_GE(stand.meanBandwidth(), before * 95 / 100) << stand.trace();
    }
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, KeyFrameKeepsTheBandwidth)
{
    Stand stand(seededPath());
    stand.run(Seconds(60));
    const qint64 before = stand.bandwidth();

    stand.setContent(Content::IDLE);
    stand.run(Seconds(10));
    stand.resetMetrics();

    for (int i = 0; i < 10; ++i)
    {
        stand.keyFrame();
        stand.run(Seconds(10));
    }

    // A key frame is a queue of the host and of the path, neither moves the publication.
    EXPECT_GE(stand.minBandwidth(), before * 9 / 10) << stand.trace();
    EXPECT_GE(stand.meanBandwidth(), before * 95 / 100) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, FirstKeyFrameMeasuresThePath)
{
    Stand stand(seededPath(), Content::IDLE);
    stand.run(Seconds(5));
    stand.resetMetrics();

    stand.keyFrame();
    stand.run(Seconds(5));

    EXPECT_GE(stand.maxBandwidth(), kCapacityApp * 7 / 10) << stand.trace();
    EXPECT_LE(stand.maxBandwidth(), kCapacityApp * 12 / 10) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, NoiseOfTheDelayUnderVideo)
{
    for (int scale : { 1, 2 })
    {
        SCOPED_TRACE(testing::Message() << "scale " << scale);

        Path path = seededPath();
        Stand stand(path);
        stand.run(Seconds(60));

        stand.setNoise(scale);
        stand.resetMetrics();
        stand.run(Seconds(600));

        EXPECT_GT(stand.minBandwidth(), kCapacityApp / 2) << stand.trace();
        EXPECT_LE(stand.shareBelow(kCapacityApp * 7 / 10), scale == 1 ? 0.01 : 0.05) << stand.trace();
    }
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, NarrowingUnderVideo)
{
    for (int divider : { 2, 5, 20 })
    {
        SCOPED_TRACE(testing::Message() << "divider " << divider);

        // The rate control fills the target, a session under the capacity shows nothing.
        Stand stand(seededPath(), Content::VIDEO, 1.0);
        stand.run(Seconds(60));

        const qint64 capacity = kCapacity / divider;
        stand.setCapacity(capacity);

        // The window, what the encoder sends until the floor, and the overflow of the bottleneck sent
        // again, at worst twice.
        const qint64 overflow = kMaxWindow - kCapacity * 250 / 1000;
        const qint64 pipe = kMaxWindow + std::llround(kCapacityApp * kPublishedShare * kBacklogBudget) +
                            2 * overflow;
        const Seconds drain(pipe / capacity + 4);
        stand.run(std::max(drain, Seconds(20)));

        EXPECT_LE(stand.bandwidth(), capacity) << stand.trace();
        stand.resetMetrics();
        stand.run(Seconds(60));
        EXPECT_GE(stand.minBandwidth(), capacity * 7 / 10) << stand.trace();
        EXPECT_LE(stand.maxBandwidth(), capacity * 13 / 10) << stand.trace();
    }
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, NarrowingWhileIdleIsFoundByTheNextVideo)
{
    Stand stand(seededPath());
    stand.run(Seconds(60));

    stand.setContent(Content::IDLE);
    stand.run(Seconds(10));

    const qint64 capacity = kCapacity / 5;
    stand.setCapacity(capacity);
    stand.run(Seconds(30));

    stand.setContent(Content::VIDEO, 1.0);
    stand.run(Seconds(10));

    stand.resetMetrics();
    stand.run(Seconds(10));
    EXPECT_LE(stand.meanBandwidth(), capacity) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, WideningIsFound)
{
    Stand stand(seededPath());
    stand.run(Seconds(60));

    const qint64 capacity = kCapacity * 3;
    stand.setCapacity(capacity);
    stand.resetMetrics();
    stand.run(Seconds(120));

    EXPECT_GE(stand.bandwidth(), capacity * 6 / 10) << stand.trace();
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, LossIsNotCongestion)
{
    for (double loss : { 0.03, 0.1 })
    {
        for (int delay_ms : { 13, 150 })
        {
            SCOPED_TRACE(testing::Message() << "loss " << loss << " delay " << delay_ms);

            Path path = seededPath();
            path.loss = loss;
            path.delay_ms = delay_ms;

            Stand stand(path);
            stand.run(Seconds(60));

            stand.resetMetrics();
            stand.run(Seconds(120));
            EXPECT_GT(stand.minBandwidth(), kMinBandwidth) << stand.trace();
        }
    }
}

//--------------------------------------------------------------------------------------------------
TEST_P(BandwidthControllerUdpStandTest, OutageRecoversAtOnce)
{
    Stand stand(seededPath());
    stand.run(Seconds(60));
    const qint64 before = stand.bandwidth();

    stand.outage(Seconds(3));
    stand.run(Seconds(10));

    stand.resetMetrics();
    stand.run(Seconds(5));
    EXPECT_GE(stand.meanBandwidth(), before * 9 / 10) << stand.trace();
}
