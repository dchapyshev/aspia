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

#include <memory>
#include <optional>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/totp.h"
#include "proto/router_admin.h"
#include "proto/router_manager.h"
#include "router/client_admin.h"
#include "router/client_manager.h"
#include "router/fake_tcp_channel.h"
#include "router/router_test_base.h"
#include "router/router_test_worker.h"
#include "router/handlers/two_factor_handler.h"
#include "router/workers/client_worker.h"

// A whole session end to end: the messages it answers, the ones it drops, and the order the stages
// come in. The session is driven through the fake channel, so nothing here mocks the router - it is
// the real ClientOperator, in a real worker thread, with a real database.
class ClientOperatorTest : public RouterTestBase
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

    // Creates the session in the worker thread, runs |body| there and destroys it there. The
    // channel belongs to the session, so it goes away with it.
    template <typename ClientT>
    void withClient(const RouterUser& user, quint32 session_type,
                    const std::function<void(ClientT&, FakeTcpChannel*)>& body)
    {
        worker_->invoke([&]()
        {
            FakeTcpChannel* channel = new FakeTcpChannel();
            channel->setPeer(user.entry_id, user.name.toStdString(), session_type, kVersion_3_0_0);

            ClientT client(worker_->database(), channel, nullptr);
            body(client, channel);
        });
    }

    // The common case: the session of the built-in administrator.
    template <typename ClientT>
    void withClient(quint32 session_type,
                    const std::function<void(ClientT&, FakeTcpChannel*)>& body)
    {
        withClient<ClientT>(admin_, session_type, body);
    }

    // The reply the session sent last, parsed as |MessageT|.
    template<typename MessageT>
    static std::optional<MessageT> lastMessage(const FakeTcpChannel* channel, quint8 channel_id)
    {
        for (int i = channel->sent().size() - 1; i >= 0; --i)
        {
            const FakeTcpChannel::Sent& sent = channel->sent().at(i);
            if (sent.channel_id != channel_id)
                continue;

            MessageT message;
            if (!parse(sent.buffer, &message))
                return std::nullopt;
            return message;
        }

        return std::nullopt;
    }

    static QByteArray totpResponse(const QString& code)
    {
        proto::router::ClientToRouter message;
        message.mutable_two_factor_response()->set_totp_code(code.toStdString());
        return serialize(message);
    }

    static QByteArray tokenResponse(const std::string& token)
    {
        proto::router::ClientToRouter message;
        message.mutable_two_factor_response()->set_token(token);
        return serialize(message);
    }

    // A code the secret does not produce anywhere inside the drift window around |at|.
    static QString wrongCode(const QByteArray& secret, qint64 at)
    {
        quint64 matched = 0;
        for (int value = 0; ; ++value)
        {
            const QString candidate = QString("%1").arg(value, Totp::kDefaultDigits, 10,
                                                        QLatin1Char('0'));
            if (!Totp::verify(secret, candidate, at, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                              Totp::kDefaultWindowSteps, &matched))
            {
                return candidate;
            }
        }
    }

    static QByteArray workspaceListRequest(qint64 request_id)
    {
        proto::router::ClientToRouter message;
        message.mutable_workspace_list_request()->set_request_id(request_id);
        return serialize(message);
    }

    static QByteArray checkHostStatusRequest(qint64 request_id, HostId host_id)
    {
        proto::router::ClientToRouter message;
        proto::router::CheckHostStatus* request = message.mutable_check_host_status();
        request->set_request_id(request_id);
        request->set_host_id(host_id);
        return serialize(message);
    }

    static QByteArray userListRequest(qint64 request_id)
    {
        proto::router::AdminToRouter message;
        proto::router::UserListRequest* request = message.mutable_user_list_request();
        request->set_request_id(request_id);
        request->set_count(proto::router::kMaxUserPageSize);
        return serialize(message);
    }

    // Clears the ENABLED flag of the user, the way an administrator disables the account.
    static void disableUser(Database& db, qint64 entry_id)
    {
        RouterUser disabled;
        disabled.entry_id = entry_id;
        disabled.flags = 0;
        ASSERT_EQ(db.modifyUser(disabled), proto::router::kErrorOk);
    }

    // Walks the session through the two-factor stage the way a client does.
    void passTwoFactor(ClientOperator* client, FakeTcpChannel* channel)
    {
        client->start();

        const std::optional<proto::router::RouterToClient> challenge =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(challenge.has_value());
        ASSERT_TRUE(challenge->has_two_factor_challenge());
        ASSERT_EQ(challenge->two_factor_challenge().mode(), proto::router::TWO_FACTOR_MODE_ACTIVE);

        channel->clearSent();
        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        const std::optional<proto::router::RouterToClient> info =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(info.has_value());
        ASSERT_TRUE(info->has_login_result());

        channel->clearSent();
    }

    // Destroyed before the database and the temporary directory of the base fixture: the worker
    // thread must be gone before the file it works with.
    WorkerManager workers_;
    RouterTestWorker* worker_ = nullptr;
    QByteArray secret_;
};

//--------------------------------------------------------------------------------------------------
// The stage opens by itself: a session that just came up asks for the second factor before it
// answers anything.
TEST_F(ClientOperatorTest, TwoFactorStageOpensOnStart)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_two_factor_challenge());
        EXPECT_EQ(message->two_factor_challenge().mode(), proto::router::TWO_FACTOR_MODE_ACTIVE);
        EXPECT_FALSE(client.isTwoFactorCompleted());
    });
}

//--------------------------------------------------------------------------------------------------
// The channel starts paused and the session lifts the pause only after the challenge is out, so
// an answer the peer pushed before the stage opened waits in the channel and passes the stage
// once the question is asked.
TEST_F(ClientOperatorTest, AnswerSentBeforeTheChallengeWaitsForIt)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [this](ClientOperator& client, FakeTcpChannel* channel)
    {
        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_TRUE(channel->isPaused());
        EXPECT_TRUE(channel->nothingSent());
        EXPECT_FALSE(client.isTwoFactorCompleted());

        client.start();

        EXPECT_FALSE(channel->isPaused());
        EXPECT_TRUE(client.isTwoFactorCompleted());

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_login_result());
    });
}

//--------------------------------------------------------------------------------------------------
// Nothing is served before the second factor: a client that skips the stage and asks for data gets
// no answer at all.
TEST_F(ClientOperatorTest, RequestsBeforeTheSecondFactorAreDropped)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT, workspaceListRequest(1));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The same rule on the privileged channels of an administrator session.
TEST_F(ClientOperatorTest, AdminRequestsBeforeTheSecondFactorAreDropped)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [](ClientAdmin& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_ADMIN, userListRequest(1));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The same rule on the manager channel: a request that would be answered with a refusal after the
// stage is not answered at all before it.
TEST_F(ClientOperatorTest, ManagerRequestsBeforeTheSecondFactorAreDropped)
{
    withClient<ClientManager>(proto::router::SESSION_TYPE_MANAGER,
                              [](ClientManager& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        proto::router::ManagerToRouter request;
        request.mutable_group_request()->set_request_id(1);
        request.mutable_group_request()->set_command_name(proto::router::kCommandGroupAdd);
        request.mutable_group_request()->set_workspace_id(0); // Invalid on purpose.

        channel->receive(proto::router::CHANNEL_ID_MANAGER, serialize(request));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// A valid code completes the stage: the answer opens the session and carries a device token for
// the next login.
TEST_F(ClientOperatorTest, ValidCodeOpensTheSessionWithAToken)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [this](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_TRUE(client.isTwoFactorCompleted());
        ASSERT_EQ(channel->sent().size(), 1);

        proto::router::RouterToClient message;
        ASSERT_TRUE(parse(channel->sent().at(0).buffer, &message));
        ASSERT_TRUE(message.has_login_result());
        EXPECT_FALSE(message.login_result().new_token().empty());
        EXPECT_EQ(message.login_result().user_id(), admin_.entry_id);
    });
}

//--------------------------------------------------------------------------------------------------
// A passed stage writes user state (the issued token, the consumed step, the enrolled secret),
// and the user lists an administrator watches must learn of it the way they learn of the admin
// commands touching the same tables.
TEST_F(ClientOperatorTest, PassedStageMarksTheUsersDirty)
{
    std::string token;

    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [&](ClientOperator& client, FakeTcpChannel* channel)
    {
        quint32 notified = 0;
        QObject::connect(&client, &ClientOperator::sig_notifyChanged,
                         [&notified](quint32 flags) { notified |= flags; });

        client.start();
        ASSERT_EQ(notified & ClientWorker::NOTIFY_USERS, 0u);
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_TRUE(notified & ClientWorker::NOTIFY_USERS);

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_login_result());
        token = message->login_result().new_token();
    });
    ASSERT_FALSE(token.empty());

    // The token path writes too - the row of the token is refreshed.
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [&](ClientOperator& client, FakeTcpChannel* channel)
    {
        quint32 notified = 0;
        QObject::connect(&client, &ClientOperator::sig_notifyChanged,
                         [&notified](quint32 flags) { notified |= flags; });

        client.start();
        channel->receive(proto::router::CHANNEL_ID_CLIENT, tokenResponse(token));

        EXPECT_TRUE(client.isTwoFactorCompleted());
        EXPECT_TRUE(notified & ClientWorker::NOTIFY_USERS);
    });
}

//--------------------------------------------------------------------------------------------------
// A wrong code ends the session instead of letting the client try again on the same connection.
// The refusal is announced by the challenge of the next session and not by this one, because an
// answer sent into a session being torn down has no delivery to rely on.
TEST_F(ClientOperatorTest, WrongCodeEndsTheConnection)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [this](ClientOperator& client, FakeTcpChannel* channel)
    {
        int finished = 0;
        QObject::connect(&client, &ClientOperator::sig_finished, [&finished](qint64) { ++finished; });

        // The owner stops the session it is told about; the stand-in here does the same, so the
        // teardown leaves the channel silenced the way it is in the worker.
        QObject::connect(&client, &ClientOperator::sig_finished, [&client](qint64) { client.stop(); });

        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(wrongCode(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_EQ(finished, 1);
        EXPECT_FALSE(client.isTwoFactorCompleted());
        EXPECT_TRUE(channel->nothingSent());
        EXPECT_TRUE(channel->isPaused());
    });

    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();

        const std::optional<proto::router::RouterToClient> challenge =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(challenge.has_value());
        ASSERT_TRUE(challenge->has_two_factor_challenge());
        EXPECT_TRUE(challenge->two_factor_challenge().code_rejected());
    });
}

//--------------------------------------------------------------------------------------------------
// The verdicts of the handler reach the client only through the serialized challenge. Without
// the flag on the wire the client would keep its dead token and present it forever.
TEST_F(ClientOperatorTest, RejectedTokenIsAnnouncedOnTheWire)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT, tokenResponse("junk"));

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_two_factor_challenge());
        EXPECT_EQ(message->two_factor_challenge().mode(), proto::router::TWO_FACTOR_MODE_ACTIVE);
        EXPECT_TRUE(message->two_factor_challenge().token_rejected());
        EXPECT_FALSE(client.isTwoFactorCompleted());
    });
}

//--------------------------------------------------------------------------------------------------
// The enrollment URI is what the operator scans, and it exists only in the serialized challenge.
TEST_F(ClientOperatorTest, EnrollmentUriIsCarriedOnTheWire)
{
    const RouterUser user = addUser("carol", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_GT(user.entry_id, 0);

    withClient<ClientOperator>(user, proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_two_factor_challenge());
        EXPECT_EQ(message->two_factor_challenge().mode(), proto::router::TWO_FACTOR_MODE_ENROLL);
        EXPECT_TRUE(QString::fromStdString(message->two_factor_challenge().otpauth_uri())
                        .startsWith("otpauth://"));
    });

    // The half-done enrollment lives in a static map and must not leak into the tests that follow.
    TwoFactorHandler::forgetUser(user.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The remainder of the block is how the client knows to show the wait instead of the code prompt,
// and it exists only in the serialized challenge.
TEST_F(ClientOperatorTest, BlockRemainderIsAnnouncedOnTheWire)
{
    const RouterUser user = addUser("dave", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_GT(user.entry_id, 0);

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(user.entry_id, secret, 0));

    // Enough misses to impose the block, straight through the handler: the wire is what this
    // test watches, and each miss over it would cost a whole session.
    setCaller(user, proto::router::SESSION_TYPE_OPERATOR);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler handler;
        proto::router::TwoFactorResponse response;
        response.set_totp_code(wrongCode(secret, now).toStdString());
        ASSERT_EQ(handler.handleResponse(db_, caller_, response, "127.0.0.1", now).action,
                  TwoFactorHandler::Action::CLOSE);
    }

    withClient<ClientOperator>(user, proto::router::SESSION_TYPE_OPERATOR,
                       [](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_two_factor_challenge());
        EXPECT_GT(message->two_factor_challenge().blocked_seconds(), 0);
    });

    // The block lives in a static map and must not leak into the tests that follow.
    TwoFactorHandler::forgetUser(user.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The SRP authenticator checks the ENABLED flag against a snapshot taken when the handshake
// started, so an account disabled while its login was in flight still reaches this stage. The
// re-read before LoginResult is where it is caught, the same way a deleted account is.
TEST_F(ClientOperatorTest, DisabledUserDoesNotFinishTheStage)
{
    const RouterUser user = addUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_GT(user.entry_id, 0);

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(user.entry_id, secret, 0));

    withClient<ClientOperator>(user, proto::router::SESSION_TYPE_OPERATOR,
                       [&](ClientOperator& client, FakeTcpChannel* channel)
    {
        int finished = 0;
        QObject::connect(&client, &ClientOperator::sig_finished, [&finished](qint64) { ++finished; });

        client.start();
        channel->clearSent();

        disableUser(worker_->database(), user.entry_id);

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret, QDateTime::currentSecsSinceEpoch())));

        EXPECT_EQ(finished, 1);
        EXPECT_FALSE(client.isTwoFactorCompleted());
        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The same rule on the token path: a stored device token of a disabled account opens nothing.
TEST_F(ClientOperatorTest, DisabledUserDoesNotPassWithAToken)
{
    const RouterUser user = addUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_GT(user.entry_id, 0);

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(user.entry_id, secret, 0));

    std::string token;
    withClient<ClientOperator>(user, proto::router::SESSION_TYPE_OPERATOR,
                       [&](ClientOperator& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret, QDateTime::currentSecsSinceEpoch())));

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_login_result());
        token = message->login_result().new_token();
    });
    ASSERT_FALSE(token.empty());

    disableUser(db_, user.entry_id);

    withClient<ClientOperator>(user, proto::router::SESSION_TYPE_OPERATOR,
                       [&](ClientOperator& client, FakeTcpChannel* channel)
    {
        int finished = 0;
        QObject::connect(&client, &ClientOperator::sig_finished, [&finished](qint64) { ++finished; });

        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT, tokenResponse(token));

        EXPECT_EQ(finished, 1);
        EXPECT_FALSE(client.isTwoFactorCompleted());
        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// After the stage the session answers, and the answer carries back the id of the request so the
// client can route it.
TEST_F(ClientOperatorTest, WorkspaceListIsAnsweredAfterTheStage)
{
    ASSERT_GT(addWorkspace("alpha"), 0);

    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_CLIENT, workspaceListRequest(77));

        const std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_workspace_list());
        EXPECT_EQ(message->workspace_list().request_id(), 77);
        EXPECT_EQ(message->workspace_list().error_code(), proto::router::kErrorOk);
        ASSERT_EQ(message->workspace_list().workspace_size(), 1);
        EXPECT_EQ(message->workspace_list().workspace(0).name(), "alpha");
    });
}

//--------------------------------------------------------------------------------------------------
// The privileged channels belong to the session types that authenticated for them: an ordinary
// client session does not answer on them, whatever it sends.
TEST_F(ClientOperatorTest, PlainClientIgnoresThePrivilegedChannels)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [this](ClientOperator& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, userListRequest(1));
        channel->receive(proto::router::CHANNEL_ID_MANAGER, userListRequest(2));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The administrator channel is answered only by an administrator session.
TEST_F(ClientOperatorTest, AdminChannelIsAnsweredByAnAdministrator)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, userListRequest(5));

        const std::optional<proto::router::RouterToAdmin> message =
            lastMessage<proto::router::RouterToAdmin>(channel, proto::router::CHANNEL_ID_ADMIN);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_user_list());
        EXPECT_EQ(message->user_list().request_id(), 5);
        EXPECT_EQ(message->user_list().error_code(), proto::router::kErrorOk);
        EXPECT_EQ(message->user_list().user_size(), 1);
    });
}

//--------------------------------------------------------------------------------------------------
// A manager session manages hosts and groups; the user, workspace and relay commands are not its
// business, and the channel they live on is not answered by it at all.
TEST_F(ClientOperatorTest, ManagerIgnoresTheAdminChannel)
{
    withClient<ClientManager>(proto::router::SESSION_TYPE_MANAGER,
                              [this](ClientManager& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, userListRequest(1));

        EXPECT_TRUE(channel->nothingSent());

        // The channel it does serve answers.
        proto::router::ManagerToRouter request;
        request.mutable_group_request()->set_request_id(2);
        request.mutable_group_request()->set_command_name(proto::router::kCommandGroupAdd);
        request.mutable_group_request()->set_workspace_id(0); // Invalid on purpose.

        channel->receive(proto::router::CHANNEL_ID_MANAGER, serialize(request));

        const std::optional<proto::router::RouterToManager> message =
            lastMessage<proto::router::RouterToManager>(channel, proto::router::CHANNEL_ID_MANAGER);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_group_result());
        EXPECT_EQ(message->group_result().request_id(), 2);
        EXPECT_EQ(message->group_result().error_code(), proto::router::kErrorInvalidRequest);
    });
}

//--------------------------------------------------------------------------------------------------
// The stage is over: a second answer to it is not a way back into it.
TEST_F(ClientOperatorTest, SecondTwoFactorResponseIsIgnored)
{
    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                       [this](ClientOperator& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_TRUE(channel->nothingSent());
        EXPECT_TRUE(client.isTwoFactorCompleted());
    });
}

//--------------------------------------------------------------------------------------------------
// An unknown command is answered - with a refusal. A request that never gets a reply would leave
// the console waiting forever.
TEST_F(ClientOperatorTest, UnknownAdminCommandIsRefusedNotIgnored)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        proto::router::AdminToRouter request;
        request.mutable_user_request()->set_request_id(9);
        request.mutable_user_request()->set_command_name("user_frobnicate");
        request.mutable_user_request()->mutable_user()->set_entry_id(admin_.entry_id);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, serialize(request));

        const std::optional<proto::router::RouterToAdmin> message =
            lastMessage<proto::router::RouterToAdmin>(channel, proto::router::CHANNEL_ID_ADMIN);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_user_result());
        EXPECT_EQ(message->user_result().request_id(), 9);
        EXPECT_EQ(message->user_result().error_code(), proto::router::kErrorInvalidRequest);
    });
}

//--------------------------------------------------------------------------------------------------
// The peer commands are refused by name like every other command of the channel. Without the check
// an unknown name would still disconnect the peer, and the answer would name a command the router
// never ran.
TEST_F(ClientOperatorTest, UnknownPeerCommandDisconnectsNobody)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        proto::router::AdminToRouter request;
        proto::router::PeerRequest* peer_request = request.mutable_peer_request();
        peer_request->set_request_id(21);
        peer_request->set_command_name("peer_frobnicate");
        peer_request->set_relay_id(1);
        peer_request->set_peer_id(2);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, serialize(request));

        const std::optional<proto::router::RouterToAdmin> message =
            lastMessage<proto::router::RouterToAdmin>(channel, proto::router::CHANNEL_ID_ADMIN);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_peer_result());
        EXPECT_EQ(message->peer_result().request_id(), 21);
        EXPECT_EQ(message->peer_result().command_name(), "peer_frobnicate");
        EXPECT_EQ(message->peer_result().error_code(), proto::router::kErrorInvalidRequest);
    });
}

//--------------------------------------------------------------------------------------------------
// Garbage on the wire is not a reason to answer or to crash.
TEST_F(ClientOperatorTest, MalformedMessagesAreIgnored)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_CLIENT, QByteArray("\xff\xfe\xfd", 3));
        channel->receive(proto::router::CHANNEL_ID_ADMIN, QByteArray("\xff\xfe\xfd", 3));
        channel->receive(200, workspaceListRequest(1)); // A channel the router does not serve.

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// Changing your own password ends every session of the user, this one included: the channel of
// each is keyed by the password that is gone, and their device tokens died with it. The client
// reconnects with the new password and runs the two-factor stage on a fresh session.
TEST_F(ClientOperatorTest, PasswordChangeEndsEverySessionOfTheUser)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        qint64 stopped_user_id = 0;

        QObject::connect(&client, &ClientOperator::sig_stopClients,
                         [&](qint64 user_id, const std::vector<qint64>&)
        {
            stopped_user_id = user_id;
        });

        const RouterUser rotated = makeUser(admin_.name, kAllSessions);

        proto::router::ClientToRouter request;
        proto::router::ChangePasswordRequest* change = request.mutable_change_password_request();
        change->set_request_id(11);
        change->set_salt(toStdString(rotated.salt));
        change->set_verifier(toStdString(rotated.verifier));

        channel->receive(proto::router::CHANNEL_ID_CLIENT, serialize(request));

        // The result of the rotation and nothing after it: the stage is not re-opened on a session
        // that is being torn down.
        ASSERT_EQ(channel->sent().size(), 1);

        proto::router::RouterToClient result;
        ASSERT_TRUE(parse(channel->sent().at(0).buffer, &result));
        ASSERT_TRUE(result.has_change_password_result());
        EXPECT_EQ(result.change_password_result().request_id(), 11);
        EXPECT_EQ(result.change_password_result().error_code(), proto::router::kErrorOk);

        // Nobody is spared, so the session that asked for the rotation goes too.
        EXPECT_EQ(stopped_user_id, admin_.entry_id);
    });
}

//--------------------------------------------------------------------------------------------------
// Neither host has a live session, so the answer comes from the database: a host that is still
// there is only offline, one that is not is reported as missing. The client keeps credentials for a
// host and drops them by that answer, so the two must not read the same.
TEST_F(ClientOperatorTest, HostStatusTellsAMissingHostFromAnOfflineOne)
{
    const HostId host_id = addHost("key-hash");
    ASSERT_NE(host_id, kInvalidHostId);

    withClient<ClientOperator>(proto::router::SESSION_TYPE_OPERATOR,
                               [this, host_id](ClientOperator& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_CLIENT, checkHostStatusRequest(11, host_id));

        std::optional<proto::router::RouterToClient> message =
            lastMessage<proto::router::RouterToClient>(channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_host_status());
        EXPECT_EQ(message->host_status().request_id(), 11);
        EXPECT_EQ(message->host_status().error_code(), proto::router::kErrorHostOffline);

        channel->clearSent();
        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         checkHostStatusRequest(12, host_id + 1000));

        message = lastMessage<proto::router::RouterToClient>(
            channel, proto::router::CHANNEL_ID_CLIENT);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_host_status());
        EXPECT_EQ(message->host_status().request_id(), 12);
        EXPECT_EQ(message->host_status().error_code(), proto::router::kErrorNotFound);
    });
}
