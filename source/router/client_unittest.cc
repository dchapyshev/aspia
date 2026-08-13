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

// A whole session end to end: the messages it answers, the ones it drops, and the order the stages
// come in. The session is driven through the fake channel, so nothing here mocks the router - it is
// the real Client, in a real worker thread, with a real database.
class ClientTest : public RouterTestBase
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
    void withClient(quint32 session_type,
                    const std::function<void(ClientT&, FakeTcpChannel*)>& body)
    {
        worker_->invoke([&]()
        {
            FakeTcpChannel* channel = new FakeTcpChannel();
            channel->setPeer(admin_.entry_id, admin_.name.toStdString(), session_type,
                             kVersion_3_0_0);

            ClientT client(worker_->database(), channel, nullptr);
            body(client, channel);
        });
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

    static QByteArray workspaceListRequest(qint64 request_id)
    {
        proto::router::ClientToRouter message;
        message.mutable_workspace_list_request()->set_request_id(request_id);
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

    // Walks the session through the two-factor stage the way a client does.
    void passTwoFactor(Client* client, FakeTcpChannel* channel)
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
        ASSERT_TRUE(info->has_user_info());

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
TEST_F(ClientTest, TwoFactorStageOpensOnStart)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [](Client& client, FakeTcpChannel* channel)
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
// Nothing is served before the second factor: a client that skips the stage and asks for data gets
// no answer at all.
TEST_F(ClientTest, RequestsBeforeTheSecondFactorAreDropped)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [](Client& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT, workspaceListRequest(1));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The same rule on the privileged channels of an administrator session.
TEST_F(ClientTest, AdminRequestsBeforeTheSecondFactorAreDropped)
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
// A valid code completes the stage: the client gets a device token for the next login and the
// identity of its account, and only then is the session usable.
TEST_F(ClientTest, ValidCodeDeliversTokenAndUserInfo)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [this](Client& client, FakeTcpChannel* channel)
    {
        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT,
                         totpResponse(Totp::code(secret_, QDateTime::currentSecsSinceEpoch())));

        EXPECT_TRUE(client.isTwoFactorCompleted());
        ASSERT_EQ(channel->sent().size(), 2);

        proto::router::RouterToClient first;
        ASSERT_TRUE(parse(channel->sent().at(0).buffer, &first));
        ASSERT_TRUE(first.has_two_factor_result());
        EXPECT_FALSE(first.two_factor_result().new_token().empty());

        proto::router::RouterToClient second;
        ASSERT_TRUE(parse(channel->sent().at(1).buffer, &second));
        ASSERT_TRUE(second.has_user_info());
        EXPECT_EQ(second.user_info().user_id(), admin_.entry_id);
        EXPECT_EQ(second.user_info().name(), admin_.name.toStdString());
    });
}

//--------------------------------------------------------------------------------------------------
// A wrong code ends the session instead of letting the client try again on the same connection.
TEST_F(ClientTest, WrongCodeEndsTheConnection)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [](Client& client, FakeTcpChannel* channel)
    {
        int finished = 0;
        QObject::connect(&client, &Client::sig_finished, [&finished](qint64) { ++finished; });

        client.start();
        channel->clearSent();

        channel->receive(proto::router::CHANNEL_ID_CLIENT, totpResponse("000000"));

        EXPECT_EQ(finished, 1);
        EXPECT_FALSE(client.isTwoFactorCompleted());
        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// After the stage the session answers, and the answer carries back the id of the request so the
// client can route it.
TEST_F(ClientTest, WorkspaceListIsAnsweredAfterTheStage)
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
TEST_F(ClientTest, PlainClientIgnoresThePrivilegedChannels)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [this](Client& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        channel->receive(proto::router::CHANNEL_ID_ADMIN, userListRequest(1));
        channel->receive(proto::router::CHANNEL_ID_MANAGER, userListRequest(2));

        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The administrator channel is answered only by an administrator session.
TEST_F(ClientTest, AdminChannelIsAnsweredByAnAdministrator)
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
TEST_F(ClientTest, ManagerIgnoresTheAdminChannel)
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
TEST_F(ClientTest, SecondTwoFactorResponseIsIgnored)
{
    withClient<Client>(proto::router::SESSION_TYPE_CLIENT,
                       [this](Client& client, FakeTcpChannel* channel)
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
TEST_F(ClientTest, UnknownAdminCommandIsRefusedNotIgnored)
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
// Garbage on the wire is not a reason to answer or to crash.
TEST_F(ClientTest, MalformedMessagesAreIgnored)
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
// Changing your own password revokes every device token, this session's included, so the router
// re-opens the two-factor stage. Until it is passed again the session answers nothing - which is
// exactly why the client must treat a challenge on a live session as a disconnect.
TEST_F(ClientTest, PasswordChangeReopensTheTwoFactorStage)
{
    withClient<ClientAdmin>(proto::router::SESSION_TYPE_ADMIN,
                            [this](ClientAdmin& client, FakeTcpChannel* channel)
    {
        passTwoFactor(&client, channel);

        const RouterUser rotated = makeUser(admin_.name, kAllSessions);

        proto::router::ClientToRouter request;
        proto::router::ChangePasswordRequest* change = request.mutable_change_password_request();
        change->set_request_id(11);
        change->set_salt(toStdString(rotated.salt));
        change->set_verifier(toStdString(rotated.verifier));

        channel->receive(proto::router::CHANNEL_ID_CLIENT, serialize(request));

        // The result of the rotation, and right after it the re-opened stage.
        ASSERT_EQ(channel->sent().size(), 2);

        proto::router::RouterToClient result;
        ASSERT_TRUE(parse(channel->sent().at(0).buffer, &result));
        ASSERT_TRUE(result.has_change_password_result());
        EXPECT_EQ(result.change_password_result().request_id(), 11);
        EXPECT_EQ(result.change_password_result().error_code(), proto::router::kErrorOk);

        proto::router::RouterToClient challenge;
        ASSERT_TRUE(parse(channel->sent().at(1).buffer, &challenge));
        ASSERT_TRUE(challenge.has_two_factor_challenge());
        EXPECT_FALSE(client.isTwoFactorCompleted());

        // The window the client must not send anything into: requests are dropped without an
        // answer.
        channel->clearSent();
        channel->receive(proto::router::CHANNEL_ID_CLIENT, workspaceListRequest(12));
        EXPECT_TRUE(channel->nothingSent());
    });
}
