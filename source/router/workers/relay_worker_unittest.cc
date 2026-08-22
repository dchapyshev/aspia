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

#include "router/workers/relay_worker.h"

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "proto/router_relay.h"
#include "router/fake_tcp_channel.h"
#include "router/relay.h"
#include "router/router_test_base.h"
#include "router/router_test_worker.h"
#include "router/workers/client_worker.h"

// Reaches the polling of the worker with a relay session put into it directly. What is under test
// is when the worker asks its relays about their sessions, not how the sessions get there.
class RelayWorkerTestPeer
{
public:
    static void addRelay(RelayWorker* worker, Relay* relay)
    {
        worker->relays_.push_back(relay);
    }

    static void fireTimer(RelayWorker* worker, TimePoint now)
    {
        worker->onTimer(now);
    }
};

// The relays of the router. A session is the real Relay in a real worker thread and only the socket
// is a stand-in.
class RelayWorkerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        std::unique_ptr<RouterTestWorker> worker = std::make_unique<RouterTestWorker>(file_path_);
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    // The socket of a relay session, with the peer facts the authenticator collects for it.
    static FakeTcpChannel* makeRelayChannel()
    {
        FakeTcpChannel* channel = new FakeTcpChannel();
        channel->setPeer(0, std::string(), proto::router::SESSION_TYPE_RELAY, kVersion_3_0_0);
        return channel;
    }

    WorkerManager workers_;
    RouterTestWorker* worker_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
// The statistics of the relays are shown to the administrators and to nobody else. With none of
// them connected the router keeps quiet, however long it runs.
TEST_F(RelayWorkerTest, RelaysAreNotPolledWithoutAdministrators)
{
    worker_->invoke([]()
    {
        RelayWorker relay_worker;
        FakeTcpChannel* channel = nullptr;
        Relay relay(channel = makeRelayChannel(), nullptr);
        relay.start();
        channel->clearSent();

        RelayWorkerTestPeer::addRelay(&relay_worker, &relay);

        const TimePoint now = Clock::now();
        RelayWorkerTestPeer::fireTimer(&relay_worker, now);
        RelayWorkerTestPeer::fireTimer(&relay_worker, now + Minutes(10));

        // Clients and managers see no statistics of the relays, so they start nothing either.
        relay_worker.onClientsChanged(ClientWorker::CLIENT_OPERATORS | ClientWorker::CLIENT_MANAGERS);
        RelayWorkerTestPeer::fireTimer(&relay_worker, now + Minutes(20));

        EXPECT_TRUE(channel->sent().isEmpty());
    });
}

//--------------------------------------------------------------------------------------------------
// An administrator gets the first list at once and a fresh one every interval afterwards, and
// the polling stops with the last of them.
TEST_F(RelayWorkerTest, RelaysArePolledWhileAnAdministratorIsConnected)
{
    static constexpr Seconds kStatisticsInterval { 5 }; // RelayWorker::requestStatistics.

    worker_->invoke([]()
    {
        RelayWorker relay_worker;
        FakeTcpChannel* channel = nullptr;
        Relay relay(channel = makeRelayChannel(), nullptr);
        relay.start();
        channel->clearSent();

        RelayWorkerTestPeer::addRelay(&relay_worker, &relay);

        // An administrator connects: waiting a whole interval would show an empty list first.
        relay_worker.onClientsChanged(ClientWorker::CLIENT_ADMINS);
        ASSERT_EQ(channel->sent().size(), 1);

        proto::router::RouterToRelay message;
        ASSERT_TRUE(parse(channel->sent().back().buffer, &message));
        EXPECT_TRUE(message.has_statistics_request());

        // A client of another kind comes and goes without disturbing the schedule.
        relay_worker.onClientsChanged(ClientWorker::CLIENT_ADMINS | ClientWorker::CLIENT_OPERATORS);
        EXPECT_EQ(channel->sent().size(), 1);

        const TimePoint now = Clock::now();
        RelayWorkerTestPeer::fireTimer(&relay_worker, now + kStatisticsInterval - Seconds(1));
        EXPECT_EQ(channel->sent().size(), 1);

        RelayWorkerTestPeer::fireTimer(&relay_worker, now + kStatisticsInterval);
        EXPECT_EQ(channel->sent().size(), 2);

        // The last administrator leaves and the relays are left alone again.
        relay_worker.onClientsChanged(ClientWorker::CLIENT_OPERATORS);
        RelayWorkerTestPeer::fireTimer(&relay_worker, now + Minutes(10));
        EXPECT_EQ(channel->sent().size(), 2);
    });
}
