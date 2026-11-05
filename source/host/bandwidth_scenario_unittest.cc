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

// The regulator on a simulated link with a capacity, a delay, a jitter and a shaper, with a session
// that encodes under the published bandwidth and pads the tests of the regulator. The tests check
// the bandwidth, its changes, the padding and the queue the user waits on.

#include "host/bandwidth_controller_tcp.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <random>

namespace {

const qint64 kMaxBandwidth = 4800 * 1024;

// The session as the host runs it (see host/client.cc).
const MilliSeconds kTick{ 1000 };
const MilliSeconds kFrameInterval{ 33 };
const qint64 kDelayProbeSize = 40;

// The encoder before the first published bandwidth, and the share of the target it fills on video.
const qint64 kDefaultTarget = 300 * 1024;
const double kEncoderFraction = 0.7;

// A static screen gets one update of this size this often. An idle session sends only the cursor.
const qint64 kStaticUpdate = 50 * 1024;
const Seconds kStaticInterval{ 3 };
const qint64 kIdleBytesPerSecond = 100;

struct Link
{
    qint64 capacity;            // Bytes per second.
    MilliSeconds delay;         // One way.
    MilliSeconds jitter;        // Standard deviation of the one way delay.
    qint64 burst = 0;           // Bytes a shaper lets through at once after an idle time.
    double capacity_noise = 0;  // Standard deviation of the capacity of a second, a share of it.
    double late_probes = 0;     // Share of the probes a retransmission holds.
};

// A retransmission waits for the round trip and four times its variance.
const MilliSeconds kRetransmissionMargin{ 30 };

enum class Content { IDLE, STATIC, VIDEO };

class Scenario
{
public:
    Scenario(const Link& link, Content content)
        : link_(link),
          content_(content),
          now_(TimePoint() + Hours(1)),
          random_(1)
    {
        // The peer's first message arrived.
        controller_.start();
    }

    void run(Seconds duration)
    {
        const TimePoint end = now_ + duration;
        while (now_ < end)
            step();
    }

    // Runs until |condition| holds, no longer than |limit|. Returns the time it took.
    Seconds runUntil(const std::function<bool()>& condition, Seconds limit)
    {
        const TimePoint start = now_;
        while (now_ - start < limit && !condition())
            step();
        return DurationCast<Seconds>(now_ - start);
    }

    void setLink(const Link& link) { link_ = link; }
    void setContent(Content content) { content_ = content; }

    // Nothing leaves the host and nothing comes back this long.
    void pause(Seconds duration) { pause_until_ = now_ + duration; }

    // Every packet gets this much extra delay this long.
    void spike(MilliSeconds extra, MilliSeconds duration)
    {
        spike_extra_ = extra;
        spike_until_ = now_ + duration;
    }

    qint64 bandwidth() const { return controller_.bandwidth(); }
    qint64 minBandwidth() const { return min_bandwidth_; }
    qint64 maxBandwidth() const { return max_bandwidth_; }
    int decreases() const { return decreases_; }
    int publications() const { return publications_; }
    qint64 paddingBytes() const { return padding_bytes_; }
    MilliSeconds maxQueueDelay() const { return max_queue_delay_; }
    proto::desktop::Overflow::State maxOverflow() const { return max_overflow_; }

    void resetMetrics()
    {
        min_bandwidth_ = controller_.bandwidth();
        max_bandwidth_ = controller_.bandwidth();
        decreases_ = 0;
        publications_ = 0;
        padding_bytes_ = 0;
        max_queue_delay_ = MilliSeconds::zero();
        max_overflow_ = proto::desktop::Overflow::STATE_NONE;
        trace_.clear();
    }

    // One line per second since the last resetMetrics(): the bandwidth, the published one, the
    // padding of the second, the queue delay and the overflow state.
    std::string trace() const { return trace_; }

private:
    enum class Kind { FRAME, PADDING, DELAY_PROBE };

    struct Item
    {
        Kind kind;
        qint64 bytes;
        qint64 remaining;
        quint32 id;
    };

    struct Arrival
    {
        TimePoint time;
        Kind kind;
        qint64 bytes;
        quint32 id;
    };

    struct Ack
    {
        TimePoint time;
        quint32 id;
    };

    void step()
    {
        now_ += MilliSeconds(1);
        ++ms_;

        drain();
        deliver();
        acks();

        if (ms_ % kFrameInterval.count() == 0)
            frame();
        if (ms_ % BandwidthController::kPaddingSlice.count() == 0)
            slice();
        if (ms_ % kTick.count() == 0)
            tick();

        max_queue_delay_ = std::max(max_queue_delay_, queueDelay());
    }

    // Client::onTimer.
    void tick()
    {
        trace_ += testing::PrintToString(ms_ / 1000) + "s bw=" +
            std::to_string(controller_.bandwidth() / 1024) + " pub=" + std::to_string(published_ / 1024) +
            " pad=" + std::to_string(padding_second_ / 1024) + " q=" +
            std::to_string(queueDelay().count()) + "ms ovf=" +
            std::to_string(controller_.overflowState(now_)) + " rtt=" +
            std::to_string(last_probe_rtt_.count()) + "\n";
        padding_second_ = 0;

        const qint64 before_tick = controller_.bandwidth();
        controller_.tick(now_);
        if (controller_.bandwidth() != before_tick)
        {
            trace_ += "    stall bw " + std::to_string(before_tick / 1024) + " -> " +
                std::to_string(controller_.bandwidth() / 1024) + "\n";
        }

        publish();

        min_bandwidth_ = std::min(min_bandwidth_, controller_.bandwidth());
        max_bandwidth_ = std::max(max_bandwidth_, controller_.bandwidth());
        max_overflow_ = std::max(max_overflow_, controller_.overflowState(now_));
    }

    // Client::publishBandwidth.
    void publish()
    {
        const qint64 bandwidth = controller_.bandwidth();
        if (!bandwidth || bandwidth == published_)
            return;

        if (published_ && bandwidth < published_)
            ++decreases_;
        ++publications_;
        published_ = bandwidth;
    }

    // The encoder sends a frame under the published bandwidth, as far as the content has anything
    // to show.
    void frame()
    {
        qint64 target = published_ ? published_ : kDefaultTarget;

        switch (content_)
        {
            case Content::VIDEO:
                target = std::llround(target * kEncoderFraction);
                pending_content_ = target;
                break;

            case Content::STATIC:
                if (now_ - last_static_update_ >= kStaticInterval)
                {
                    pending_content_ += kStaticUpdate;
                    last_static_update_ = now_;
                }
                break;

            case Content::IDLE:
                break;
        }

        qint64 bytes = std::min(pending_content_, target * kFrameInterval.count() / 1000);
        pending_content_ -= bytes;
        bytes += kIdleBytesPerSecond * kFrameInterval.count() / 1000;

        enqueue(Kind::FRAME, bytes, 0);
    }

    // Client::onSliceTimer.
    void slice()
    {
        if (controller_.startProbe(now_))
            sendDelayProbe();

        const qint64 due = controller_.paddingDue(now_);
        if (due <= 0)
            return;

        padding_bytes_ += due;
        padding_second_ += due;
        enqueue(Kind::PADDING, due, 0);
    }

    void sendDelayProbe()
    {
        delay_probe_id_ = ++next_id_;
        delay_probe_sent_ = now_;
        enqueue(Kind::DELAY_PROBE, kDelayProbeSize, delay_probe_id_);
    }

    // Client::send and Client::sendControl.
    void enqueue(Kind kind, qint64 bytes, quint32 id)
    {
        controller_.onBytesSent(bytes);
        queue_.push_back({ kind, bytes, bytes, id });
    }

    // The link takes bytes from the host queue at its capacity, which varies from second to second.
    // A shaper saves the credit of an idle time up to its burst and lets that much through at once.
    void drain()
    {
        if (link_.capacity_noise > 0 && ms_ % kTick.count() == 0)
        {
            std::normal_distribution<double> noise(1, link_.capacity_noise);
            capacity_factor_ = std::max(0.0, noise(random_));
        }

        if (now_ < pause_until_)
            return;

        credit_ += link_.capacity * capacity_factor_ / 1000;

        while (!queue_.empty() && credit_ >= queue_.front().remaining)
        {
            const Item item = queue_.front();
            queue_.pop_front();
            credit_ -= item.remaining;

            // In order delivery: a packet does not overtake the previous one.
            last_arrival_ = std::max(afterDelay(now_), last_arrival_);
            arrivals_.push_back({ last_arrival_, item.kind, item.bytes, item.id });
        }

        if (!queue_.empty())
        {
            queue_.front().remaining -= static_cast<qint64>(credit_);
            credit_ -= static_cast<qint64>(credit_);
        }
        else
        {
            credit_ = std::min(credit_, static_cast<double>(link_.burst));
        }
    }

    // The client receives, measures and acks.
    void deliver()
    {
        while (!arrivals_.empty() && arrivals_.front().time <= now_)
        {
            const Arrival arrival = arrivals_.front();
            arrivals_.pop_front();

            if (arrival.kind != Kind::DELAY_PROBE)
                continue;

            // The probe or its ack was lost and sent again.
            TimePoint ack = afterDelay(arrival.time);
            if (link_.late_probes > 0 && std::bernoulli_distribution(link_.late_probes)(random_))
                ack += link_.delay * 2 + kRetransmissionMargin;

            scheduleAck({ ack, arrival.id });
        }
    }

    void scheduleAck(Ack ack)
    {
        // The way back is in order too, and nothing comes back during a pause.
        ack.time = std::max({ ack.time, last_ack_, pause_until_ });
        last_ack_ = ack.time;
        acks_.push_back(ack);
    }

    // Client::onBandwidthProbeAck.
    void acks()
    {
        while (!acks_.empty() && acks_.front().time <= now_)
        {
            const Ack ack = acks_.front();
            acks_.pop_front();

            if (ack.id != delay_probe_id_)
                continue;

            last_probe_rtt_ = DurationCast<MilliSeconds>(now_ - delay_probe_sent_);
            const qint64 before = controller_.bandwidth();
            controller_.finishProbe(now_);
            if (controller_.bandwidth() != before)
            {
                trace_ += "    probe rtt=" + std::to_string(last_probe_rtt_.count()) + "ms bw " +
                    std::to_string(before / 1024) + " -> " +
                    std::to_string(controller_.bandwidth() / 1024) + "\n";
            }

            publish();
        }
    }

    TimePoint afterDelay(TimePoint from)
    {
        MilliSeconds delay = link_.delay;
        if (now_ < spike_until_)
            delay += spike_extra_;
        if (link_.jitter > MilliSeconds::zero())
        {
            std::normal_distribution<double> jitter(0, static_cast<double>(link_.jitter.count()));
            delay += MilliSeconds(std::max<qint64>(0, static_cast<qint64>(std::lround(jitter(random_)))));
        }
        return from + delay;
    }

    // How long the data queued at the host takes to leave it.
    MilliSeconds queueDelay() const
    {
        qint64 queued = 0;
        for (const Item& item : queue_)
            queued += item.remaining;

        MilliSeconds result(queued * 1000 / link_.capacity);
        if (now_ < pause_until_)
            result += DurationCast<MilliSeconds>(pause_until_ - now_);
        return result;
    }

    Link link_;
    Content content_;
    TimePoint now_;
    qint64 ms_ = 0;
    std::mt19937 random_;

    BandwidthControllerTcp controller_;

    std::deque<Item> queue_;
    double credit_ = 0;
    double capacity_factor_ = 1;
    std::deque<Arrival> arrivals_;
    TimePoint last_arrival_;
    std::deque<Ack> acks_;
    TimePoint last_ack_;
    TimePoint pause_until_;
    TimePoint spike_until_;
    MilliSeconds spike_extra_{ 0 };

    quint32 next_id_ = 0;
    quint32 delay_probe_id_ = 0;
    TimePoint delay_probe_sent_;
    MilliSeconds last_probe_rtt_{ 0 };
    qint64 published_ = 0;
    qint64 padding_second_ = 0;
    qint64 pending_content_ = 0;
    TimePoint last_static_update_;

    qint64 min_bandwidth_ = 0;
    qint64 max_bandwidth_ = 0;
    int decreases_ = 0;
    int publications_ = 0;
    qint64 padding_bytes_ = 0;
    MilliSeconds max_queue_delay_{ 0 };
    proto::desktop::Overflow::State max_overflow_ = proto::desktop::Overflow::STATE_NONE;
    std::string trace_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(BandwidthScenarioTest, LanStaysAtCeiling)
{
    Scenario scenario({ 100 * 1024 * 1024, MilliSeconds(1), MilliSeconds(0) }, Content::VIDEO);

    scenario.run(Seconds(30));
    EXPECT_EQ(scenario.bandwidth(), kMaxBandwidth) << scenario.trace();

    scenario.resetMetrics();
    scenario.run(Seconds(120));
    EXPECT_EQ(scenario.minBandwidth(), kMaxBandwidth) << scenario.trace();
    EXPECT_EQ(scenario.decreases(), 0) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// A session with endless content settles near the capacity of the link: within the AIMD band, with
// few steps down, and without a queue the user would notice.
TEST(BandwidthScenarioTest, TracksLinkCapacity)
{
    const qint64 capacities[] = { 50 * 1024, 300 * 1024, 1024 * 1024, 3 * 1024 * 1024 };
    const MilliSeconds delays[] = { MilliSeconds(20), MilliSeconds(50), MilliSeconds(100) };

    for (qint64 capacity : capacities)
    {
        for (MilliSeconds delay : delays)
        {
            SCOPED_TRACE(testing::Message() << "capacity " << (capacity / 1024) << " kB/s, delay "
                                            << delay.count() << " ms");

            Scenario scenario({ capacity, delay, MilliSeconds(0) }, Content::VIDEO);
            scenario.run(Seconds(60));

            scenario.resetMetrics();
            scenario.run(Seconds(60));
            EXPECT_GE(scenario.minBandwidth(), capacity * 7 / 10) << scenario.trace();
            EXPECT_LE(scenario.maxBandwidth(), capacity * 12 / 10) << scenario.trace();

            // The bandwidth climbs from 0.85 of the capacity by 8% a second, overuses it and steps
            // back. A step down more often than every 8 s is a saw the user sees.
            EXPECT_LE(scenario.decreases(), 8) << scenario.trace();
            EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
        }
    }
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthScenarioTest, FollowsCapacityDrop)
{
    const qint64 kCapacity = 1024 * 1024;
    const qint64 kDropped = 200 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0) }, Content::VIDEO);
    scenario.run(Seconds(60));

    scenario.setLink({ kDropped, MilliSeconds(50), MilliSeconds(0) });
    const Seconds reaction = scenario.runUntil(
        [&scenario]() { return scenario.bandwidth() <= kDropped; }, Seconds(30));
    EXPECT_LE(reaction.count(), 5) << scenario.trace();

    // The queue built before the reaction drains, then the session settles on the new capacity.
    scenario.run(Seconds(30));
    scenario.resetMetrics();
    scenario.run(Seconds(60));
    EXPECT_GE(scenario.minBandwidth(), kDropped * 7 / 10) << scenario.trace();
    EXPECT_LE(scenario.maxBandwidth(), kDropped * 12 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthScenarioTest, FollowsCapacityRise)
{
    const qint64 kCapacity = 200 * 1024;
    const qint64 kRaised = 1024 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0) }, Content::VIDEO);
    scenario.run(Seconds(60));

    scenario.setLink({ kRaised, MilliSeconds(50), MilliSeconds(0) });
    const Seconds recovery = scenario.runUntil(
        [&scenario]() { return scenario.bandwidth() >= kRaised * 8 / 10; }, Seconds(120));
    EXPECT_LE(recovery.count(), 60) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// A delay that comes from the network and not from the session must not cost the user quality. The
// regulator steps down now and then by itself while it looks for more bandwidth, so each spike is
// compared with the same session without it.
TEST(BandwidthScenarioTest, DelaySpikesUnderVideo)
{
    const Link kLink = { 1024 * 1024, MilliSeconds(20), MilliSeconds(0) };

    struct Spike
    {
        const char* name;
        MilliSeconds extra;
        MilliSeconds duration;
        Seconds run;
        int extra_decreases;
    };

    const Spike spikes[] =
    {
        // One late packet. It is the probe leaving right now, and the in order delivery holds the
        // next 300 ms of packets behind it.
        { "single", MilliSeconds(300), MilliSeconds(10), Seconds(10), 0 },

        // A delay that stays and does not grow, 100 ms each way and 200 ms on the round trip of a
        // probe.
        { "plateau", MilliSeconds(100), MilliSeconds(5000), Seconds(15), 0 },

        // Two late probes in a row look like a queue, the bandwidth may step down once.
        { "two probes", MilliSeconds(200), MilliSeconds(2500), Seconds(40), 1 },
    };

    for (const Spike& spike : spikes)
    {
        SCOPED_TRACE(spike.name);

        Scenario scenario(kLink, Content::VIDEO);
        Scenario baseline(kLink, Content::VIDEO);
        scenario.run(Seconds(60));
        baseline.run(Seconds(60));

        scenario.resetMetrics();
        baseline.resetMetrics();
        scenario.spike(spike.extra, spike.duration);
        scenario.run(spike.run);
        baseline.run(spike.run);

        EXPECT_LE(scenario.decreases(), baseline.decreases() + spike.extra_decreases)
            << scenario.trace() << "without the spike:\n" << baseline.trace();
        EXPECT_GE(scenario.bandwidth(), baseline.bandwidth() * 9 / 10)
            << scenario.trace() << "without the spike:\n" << baseline.trace();
    }
}

//--------------------------------------------------------------------------------------------------
// The same spikes while the user reads: nothing is sent, the bandwidth must be there when the user
// moves again.
TEST(BandwidthScenarioTest, DelaySpikesWhileIdle)
{
    Scenario scenario({ 1024 * 1024, MilliSeconds(20), MilliSeconds(0) }, Content::VIDEO);
    scenario.run(Seconds(60));
    const qint64 before = scenario.bandwidth();

    scenario.setContent(Content::IDLE);
    scenario.run(Seconds(10));

    scenario.spike(MilliSeconds(200), MilliSeconds(2500));
    scenario.run(Seconds(10));
    EXPECT_GE(scenario.bandwidth(), before * 8 / 10) << scenario.trace();

    scenario.setContent(Content::VIDEO);
    scenario.run(Seconds(3));
    EXPECT_GE(scenario.bandwidth(), before * 8 / 10) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// A static screen sends far less than the bandwidth, the padding of the tests finds the capacity.
TEST(BandwidthScenarioTest, StaticScreenFindsTheCapacity)
{
    const qint64 kCapacity = 1024 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0) }, Content::STATIC);
    scenario.run(Seconds(60));

    scenario.resetMetrics();
    scenario.run(Seconds(60));
    EXPECT_GE(scenario.minBandwidth(), kCapacity * 7 / 10) << scenario.trace();
    EXPECT_LE(scenario.maxBandwidth(), kCapacity * 12 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// The bandwidth found on a static screen is a rate the link carried, so the video that follows
// does not overflow it.
TEST(BandwidthScenarioTest, VideoAfterStaticScreenDoesNotOverflow)
{
    const qint64 kCapacity = 250 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0) }, Content::STATIC);
    scenario.run(Seconds(30));

    scenario.setContent(Content::VIDEO);
    scenario.resetMetrics();
    scenario.run(Seconds(30));
    EXPECT_GE(scenario.minBandwidth(), kCapacity * 7 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// The session sends under the held capacity, so only a test over it finds the wider path. The test
// comes at most 60 s after the previous one, and the climb from 50 kB/s to the ceiling takes 26 s at
// a step of 1.2 a second.
TEST(BandwidthScenarioTest, LanAfterNarrowLink)
{
    Scenario scenario({ 50 * 1024, MilliSeconds(20), MilliSeconds(0) }, Content::VIDEO);
    scenario.run(Seconds(60));

    scenario.setLink({ 100 * 1024 * 1024, MilliSeconds(1), MilliSeconds(0) });
    const Seconds recovery = scenario.runUntil(
        [&scenario]() { return scenario.bandwidth() >= kMaxBandwidth; }, Seconds(120));
    EXPECT_LE(recovery.count(), 60 + 26) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// The tests cost the climb to the ceiling once, and a second over the capacity every ten seconds on
// a link that was measured.
TEST(BandwidthScenarioTest, PaddingIsBounded)
{
    Scenario lan({ 100 * 1024 * 1024, MilliSeconds(1), MilliSeconds(0) }, Content::STATIC);
    lan.run(Seconds(60));
    EXPECT_LE(lan.paddingBytes(), 40 * 1024 * 1024) << lan.trace();

    lan.resetMetrics();
    lan.run(Seconds(60));
    EXPECT_EQ(lan.paddingBytes(), 0) << lan.trace();

    const qint64 kCapacity = 250 * 1024;
    Scenario narrow({ kCapacity, MilliSeconds(50), MilliSeconds(0) }, Content::STATIC);
    narrow.run(Seconds(60));

    narrow.resetMetrics();
    narrow.run(Seconds(60));
    EXPECT_LE(narrow.paddingBytes(), kCapacity * 8) << narrow.trace();
}

//--------------------------------------------------------------------------------------------------
// The link of the test machine, 10 Mbit/s shaped with a bucket and 50 ms each way.
TEST(BandwidthScenarioTest, ShapedLinkUnderVideo)
{
    const qint64 kCapacity = 1220 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0), 64 * 1024 }, Content::VIDEO);
    scenario.run(Seconds(60));

    scenario.resetMetrics();
    scenario.run(Seconds(60));
    EXPECT_GE(scenario.minBandwidth(), kCapacity * 7 / 10) << scenario.trace();
    EXPECT_LE(scenario.maxBandwidth(), kCapacity * 12 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
TEST(BandwidthScenarioTest, ShapedLinkIdle)
{
    const qint64 kCapacity = 1220 * 1024;

    Scenario scenario({ kCapacity, MilliSeconds(50), MilliSeconds(0), 64 * 1024 }, Content::IDLE);
    scenario.run(Seconds(60));

    scenario.resetMetrics();
    scenario.run(Seconds(60));
    EXPECT_GE(scenario.minBandwidth(), kCapacity * 7 / 10) << scenario.trace();
    EXPECT_LE(scenario.maxBandwidth(), kCapacity * 12 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}

//--------------------------------------------------------------------------------------------------
// A lossy link the transport carries at a capacity that varies from second to second, with probes
// held by retransmissions. The acceptance of the regulator: the bandwidth within 70-110% of the
// capacity and no overflow under the steady load.
TEST(BandwidthScenarioTest, LossyLinkUnderVideo)
{
    const qint64 kCapacity = 1390 * 1024;

    Link link{ kCapacity, MilliSeconds(150), MilliSeconds(0) };
    link.capacity_noise = 0.04;
    link.late_probes = 0.04;

    Scenario scenario(link, Content::VIDEO);
    scenario.run(Seconds(60));

    scenario.resetMetrics();
    scenario.run(Seconds(300));
    EXPECT_GE(scenario.minBandwidth(), kCapacity * 7 / 10) << scenario.trace();
    EXPECT_LE(scenario.maxBandwidth(), kCapacity * 11 / 10) << scenario.trace();
    EXPECT_EQ(scenario.maxOverflow(), proto::desktop::Overflow::STATE_NONE) << scenario.trace();
}
