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

#include "router/workers/client_worker.h"

#include <QDateTime>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/totp.h"
#include "router/client_admin.h"
#include "router/client_operator.h"
#include "router/fake_tcp_channel.h"
#include "router/router_test_base.h"
#include "router/router_test_worker.h"
#include "router/handlers/two_factor_handler.h"

namespace {

// The live sessions of a router: two ordinary ones and the administrator console that sends the
// commands (the last id).
const std::vector<qint64> kSessions = { 1, 2, 7 };
constexpr qint64 kAdminSession = 7;
constexpr qint64 kAllSessions = -1;

} // namespace

//--------------------------------------------------------------------------------------------------
// "Disconnect everybody" must leave the console that asked for it alone: the administrator would
// otherwise drop its own connection and have to pass the two-factor stage again to see the result
// of its own command. Every other command of the admin channel that acts on sessions follows the
// same rule.
TEST(ClientWorkerTest, DisconnectAllSpareTheRequestingSession)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop(kSessions, kAllSessions, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// Nothing else is connected: the command has nothing to do, which is not a failure.
TEST(ClientWorkerTest, DisconnectAllWithNobodyElseSelectsNothing)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop({ kAdminSession }, kAllSessions, kAdminSession);

    EXPECT_TRUE(targets.empty());
}

//--------------------------------------------------------------------------------------------------
TEST(ClientWorkerTest, DisconnectOneTargetsExactlyThatSession)
{
    const std::vector<qint64> targets = ClientWorker::sessionsToStop(kSessions, 2, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ 2 }));
}

//--------------------------------------------------------------------------------------------------
// Picking your own session in the list is an explicit decision, so it is carried out.
TEST(ClientWorkerTest, OwnSessionCanBePickedExplicitly)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop(kSessions, kAdminSession, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ kAdminSession }));
}

//--------------------------------------------------------------------------------------------------
// The session ended between the list and the command: nothing is selected, and the caller answers
// with an invalid entry id instead of silently reporting success.
TEST(ClientWorkerTest, UnknownSessionSelectsNothing)
{
    EXPECT_TRUE(ClientWorker::sessionsToStop(kSessions, 12345, kAdminSession).empty());
    EXPECT_TRUE(ClientWorker::sessionsToStop({}, 1, kAdminSession).empty());
}

// The list of live sessions, the stop command and the timer are private to the worker; the peer
// reaches them so they can be exercised against real sessions.
class ClientWorkerTestPeer
{
public:
    static std::vector<ClientOperator*>& clients(ClientWorker& worker) { return worker.clients_; }

    static void stopClients(ClientWorker& worker, qint64 user_id, const std::vector<qint64>& token_ids)
    {
        worker.onStopClients(user_id, token_ids);
    }

    static void fireTimer(ClientWorker& worker, TimePoint now) { worker.onTimer(now); }
    static void notifyChanged(ClientWorker& worker, quint32 flags) { worker.onNotifyChanged(flags); }
    static void updateClientsMask(ClientWorker& worker) { worker.updateClientsMask(); }

    // The removal of a session that ended on its own, wired the way onNewConnection does it.
    // The connection is made direct explicitly, because the worker of the stand is never
    // started and sits on its own thread, so an automatic connection would queue forever.
    static void connectFinished(ClientWorker& worker, ClientOperator* client)
    {
        QObject::connect(client, &ClientOperator::sig_finished,
                         &worker, &ClientWorker::onSessionFinished, Qt::DirectConnection);
    }
    static quint32 clientsMask(ClientWorker& worker) { return worker.clients_mask_; }
    static void stop(ClientWorker& worker) { worker.onStop(); }
};

// The stop command against live sessions: which of them the pair (user, token list) takes down.
// The sessions are the real ClientOperator over the fake channel, created and stopped in the
// worker thread the way the router does it. The worker object itself is never started: the
// command is a plain call, and the thread the sessions need is the test worker's.
class ClientWorkerStopTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        secret_ = Totp::generateSecret();
        ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret_, 0));

        std::unique_ptr<RouterTestWorker> worker = std::make_unique<RouterTestWorker>(file_path_);
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    // A session of |user| held by the worker. Of the wiring onNewConnection sets up, the stand
    // reproduces the mask update on passing the stage and the removal on sig_finished. A negative
    // |code_time| leaves the session at the two-factor stage; otherwise the stage is passed with
    // the code of that step (the secret of the built-in administrator). Runs in the worker thread.
    template <typename ClientT = ClientOperator>
    ClientT* addSession(ClientWorker& worker, const RouterUser& user, qint64 code_time,
                        FakeTcpChannel** channel_out = nullptr,
                        quint32 session_type = proto::router::SESSION_TYPE_OPERATOR)
    {
        FakeTcpChannel* channel = new FakeTcpChannel();
        channel->setPeer(user.entry_id, user.name.toStdString(), session_type,
                         kVersion_3_0_0);
        if (channel_out)
            *channel_out = channel;

        ClientT* client = new ClientT(worker_->database(), channel, nullptr);
        ClientWorkerTestPeer::clients(worker).push_back(client);
        QObject::connect(client, &ClientOperator::sig_twoFactorCompleted, client,
                         [&worker]() { ClientWorkerTestPeer::updateClientsMask(worker); });
        ClientWorkerTestPeer::connectFinished(worker, client);

        client->start();

        if (code_time >= 0)
        {
            proto::router::ClientToRouter message;
            message.mutable_two_factor_response()->set_totp_code(
                Totp::code(secret_, code_time).toStdString());
            channel->receive(proto::router::CHANNEL_ID_CLIENT, serialize(message));
            EXPECT_TRUE(client->isTwoFactorCompleted());
        }

        return client;
    }

    static bool holds(ClientWorker& worker, const ClientOperator* client)
    {
        const std::vector<ClientOperator*>& clients = ClientWorkerTestPeer::clients(worker);
        return std::ranges::find(clients, client) != clients.end();
    }

    // Destroyed before the database and the temporary directory of the base fixture: the worker
    // thread must be gone before the file it works with.
    WorkerManager workers_;
    RouterTestWorker* worker_ = nullptr;
    QByteArray secret_;
};

//--------------------------------------------------------------------------------------------------
// A revocation of specific tokens stops only the sessions opened with them. The one still at the
// two-factor stage holds no token yet and stays, and so does the session of another token.
TEST_F(ClientWorkerStopTest, TokenRevocationStopsOnlyTheListedSession)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        ClientOperator* revoked = addSession(worker, admin_, now);
        ClientOperator* kept = addSession(worker, admin_, now + Totp::kDefaultStepSec);
        ClientOperator* at_stage = addSession(worker, admin_, -1);

        ASSERT_GT(revoked->tokenId(), 0);
        ASSERT_GT(kept->tokenId(), 0);
        ASSERT_EQ(at_stage->tokenId(), 0);

        ClientWorkerTestPeer::stopClients(worker, admin_.entry_id, { revoked->tokenId() });

        EXPECT_FALSE(holds(worker, revoked));
        EXPECT_TRUE(holds(worker, kept));
        EXPECT_TRUE(holds(worker, at_stage));

        ClientWorkerTestPeer::stop(worker);
    });
}

//--------------------------------------------------------------------------------------------------
// An empty token list means the whole user: every session of theirs goes, the one still at the
// two-factor stage included, and the sessions of other users stay.
TEST_F(ClientWorkerStopTest, FullStopTakesEverySessionOfTheUser)
{
    const RouterUser bob = addUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_GT(bob.entry_id, 0);

    worker_->invoke([&]()
    {
        ClientWorker worker;

        ClientOperator* logged_in = addSession(worker, admin_, QDateTime::currentSecsSinceEpoch());
        ClientOperator* at_stage = addSession(worker, admin_, -1);
        ClientOperator* other_user = addSession(worker, bob, -1);

        ClientWorkerTestPeer::stopClients(worker, admin_.entry_id, {});

        EXPECT_FALSE(holds(worker, logged_in));
        EXPECT_FALSE(holds(worker, at_stage));
        EXPECT_TRUE(holds(worker, other_user));

        ClientWorkerTestPeer::stop(worker);
    });

    // The enrollment the session of bob opened lives in a static map and must not leak into the
    // tests that follow.
    TwoFactorHandler::forgetUser(bob.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The channel error is the only notice the worker gets of a socket that died under a session,
// and it must take exactly that session out of the list.
TEST_F(ClientWorkerStopTest, SocketDeathDropsOnlyTheDeadSession)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        FakeTcpChannel* channel = nullptr;
        ClientOperator* dying = addSession(worker, admin_, now, &channel);
        ClientOperator* kept = addSession(worker, admin_, now + Totp::kDefaultStepSec);

        channel->fail(TcpChannel::ErrorCode::REMOTE_HOST_CLOSED);

        EXPECT_FALSE(holds(worker, dying));
        EXPECT_TRUE(holds(worker, kept));

        ClientWorkerTestPeer::stop(worker);
    });
}

//--------------------------------------------------------------------------------------------------
// The stage does not wait forever: the timer drops a session that sits at the second factor past
// the timeout and leaves the ones that passed it alone.
TEST_F(ClientWorkerStopTest, StageTimeoutDropsOnlyTheStuckSession)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        ClientOperator* ready = addSession(worker, admin_, QDateTime::currentSecsSinceEpoch());
        ClientOperator* stuck = addSession(worker, admin_, -1);

        // Not enough time has passed: both stay.
        ClientWorkerTestPeer::fireTimer(worker, Clock::now());
        EXPECT_TRUE(holds(worker, ready));
        EXPECT_TRUE(holds(worker, stuck));

        // Past the two-minute stage timeout the stuck one goes.
        ClientWorkerTestPeer::fireTimer(worker, Clock::now() + Minutes(3));
        EXPECT_TRUE(holds(worker, ready));
        EXPECT_FALSE(holds(worker, stuck));

        ClientWorkerTestPeer::stop(worker);
    });
}

//--------------------------------------------------------------------------------------------------
// The periodic notifications are data of the session, so they wait for the second factor the same
// way the requests do.
TEST_F(ClientWorkerStopTest, NotificationsWaitForTheSecondFactor)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        FakeTcpChannel* ready_channel = nullptr;
        FakeTcpChannel* stuck_channel = nullptr;
        addSession(worker, admin_, QDateTime::currentSecsSinceEpoch(), &ready_channel);
        addSession(worker, admin_, -1, &stuck_channel);

        ready_channel->clearSent();
        stuck_channel->clearSent();

        ClientWorkerTestPeer::notifyChanged(worker, ClientWorker::NOTIFY_HOSTS);
        ClientWorkerTestPeer::fireTimer(worker, Clock::now());

        ASSERT_EQ(ready_channel->sent().size(), 1);
        proto::router::RouterToClient message;
        ASSERT_TRUE(parse(ready_channel->sent().at(0).buffer, &message));
        ASSERT_TRUE(message.has_notification());
        EXPECT_TRUE(message.notification().hosts_dirty());

        EXPECT_TRUE(stuck_channel->nothingSent());

        ClientWorkerTestPeer::stop(worker);
    });
}

//--------------------------------------------------------------------------------------------------
// The user lists only the administrators watch ride the admin flush. The notification of an
// administrator carries the bit, and an operator hears nothing of it at all.
TEST_F(ClientWorkerStopTest, UsersDirtyReachesTheAdminSessions)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        FakeTcpChannel* admin_channel = nullptr;
        FakeTcpChannel* operator_channel = nullptr;
        addSession<ClientAdmin>(worker, admin_, now, &admin_channel,
                                proto::router::SESSION_TYPE_ADMIN);
        addSession(worker, admin_, now + Totp::kDefaultStepSec, &operator_channel);

        admin_channel->clearSent();
        operator_channel->clearSent();

        ClientWorkerTestPeer::notifyChanged(worker, ClientWorker::NOTIFY_USERS);
        ClientWorkerTestPeer::fireTimer(worker, Clock::now());

        ASSERT_EQ(admin_channel->sent().size(), 1);
        proto::router::RouterToClient message;
        ASSERT_TRUE(parse(admin_channel->sent().at(0).buffer, &message));
        ASSERT_TRUE(message.has_notification());
        EXPECT_TRUE(message.notification().users_dirty());

        EXPECT_TRUE(operator_channel->nothingSent());

        ClientWorkerTestPeer::stop(worker);
    });
}

//--------------------------------------------------------------------------------------------------
// The kinds mask feeds the relay statistics polling, so it counts only the sessions that passed
// the second factor. A peer that holds the password alone must not switch that work on, and the
// mask catches up once the stage is passed.
TEST_F(ClientWorkerStopTest, ClientsMaskWaitsForTheSecondFactor)
{
    worker_->invoke([&]()
    {
        ClientWorker worker;

        FakeTcpChannel* channel = nullptr;
        ClientOperator* client = addSession(worker, admin_, -1, &channel);
        ClientWorkerTestPeer::updateClientsMask(worker);
        EXPECT_EQ(ClientWorkerTestPeer::clientsMask(worker), 0u);

        proto::router::ClientToRouter message;
        message.mutable_two_factor_response()->set_totp_code(
            Totp::code(secret_, QDateTime::currentSecsSinceEpoch()).toStdString());
        channel->receive(proto::router::CHANNEL_ID_CLIENT, serialize(message));

        ASSERT_TRUE(client->isTwoFactorCompleted());
        EXPECT_EQ(ClientWorkerTestPeer::clientsMask(worker),
                  quint32(ClientWorker::CLIENT_OPERATORS));

        ClientWorkerTestPeer::stop(worker);
    });
}
