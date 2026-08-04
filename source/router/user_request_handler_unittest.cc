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

#include "router/user_request_handler.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <vector>

#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workers/client_worker.h"
#include "router/workspace.h"

namespace {

constexpr quint32 kAllSessions = proto::router::SESSION_TYPE_ADMIN |
    proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_CLIENT;

//--------------------------------------------------------------------------------------------------
RouterUser makeUser(const QString& name, quint32 sessions)
{
    RouterUser user = RouterUser::create(name, SecureString(QStringLiteral("Password1234!")));
    user.sessions = sessions;
    user.flags = User::ENABLED;
    return user;
}

//--------------------------------------------------------------------------------------------------
std::string toStdString(const QByteArray& bytes)
{
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

} // namespace

// The whole user surface of the admin channel, exercised against an isolated database in the state
// --create-config leaves behind: the built-in administrator with id 1.
class UserRequestHandlerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        ASSERT_TRUE(db_.open(temp_dir_.path() + "/router.db3"));

        ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("admin"), kAllSessions)),
                  proto::router::kErrorOk);
        admin_ = db_.findUser(QStringLiteral("admin"));
        ASSERT_EQ(admin_.entry_id, 1);

        caller_.user_id = admin_.entry_id;
        caller_.name = admin_.name;
    }

    UserRequestHandler::Result handle(const proto::router::UserRequest& request)
    {
        return UserRequestHandler::handle(db_, caller_, request);
    }

    // A request carrying the full record: name, credentials and flags.
    proto::router::UserRequest makeRequest(std::string_view command, const RouterUser& user)
    {
        proto::router::UserRequest request;
        request.set_command_name(std::string(command));
        request.mutable_user()->CopyFrom(user.serialize());
        return request;
    }

    // A request that only carries the id (delete, OTP reset, token revocation).
    proto::router::UserRequest makeIdRequest(std::string_view command, qint64 user_id)
    {
        proto::router::UserRequest request;
        request.set_command_name(std::string(command));
        request.mutable_user()->set_entry_id(user_id);
        return request;
    }

    // A workspace whose group key is sealed for the built-in administrator.
    qint64 addWorkspace(const QString& name, const SecureByteArray& gk)
    {
        Workspace::Access access;
        access.user_id = admin_.entry_id;
        access.wrapped_gk = toStdString(SealedBox::seal(gk, admin_.public_key));
        access.public_key = toStdString(admin_.public_key);

        qint64 entry_id = -1;
        if (db_.addWorkspace(name.toStdString(), std::string_view(), {access}, {}, &entry_id) !=
            proto::router::kErrorOk)
        {
            return -1;
        }

        return entry_id;
    }

    qint64 issueToken(qint64 user_id)
    {
        std::string token;
        qint64 token_id = 0;
        if (!db_.issueClientDeviceToken(user_id, "127.0.0.1", &token, &token_id))
            return -1;
        return token_id;
    }

    size_t tokenCount(qint64 user_id)
    {
        std::vector<DeviceToken> tokens;
        if (!db_.listClientDeviceTokens(user_id, &tokens))
            return static_cast<size_t>(-1);
        return tokens.size();
    }

    QTemporaryDir temp_dir_;
    Database db_;
    RouterUser admin_;
    RequestCaller caller_;
};

//--------------------------------------------------------------------------------------------------
// A user that is not an administrator changes nothing about the workspaces, so only the user list
// is announced as stale.
TEST_F(UserRequestHandlerTest, AddClientNotifiesUsersOnly)
{
    const UserRequestHandler::Result result = handle(makeRequest(
        proto::router::kCommandUserAdd,
        makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_TRUE(db_.findUser(QStringLiteral("bob")).isValid());
}

//--------------------------------------------------------------------------------------------------
// A new administrator receives an access entry for every workspace from the keys of the request,
// so the workspace lists of the other sessions become stale as well.
TEST_F(UserRequestHandlerTest, AddAdminGrantsWorkspacesAndNotifies)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk);
    ASSERT_GT(workspace_id, 0);

    const RouterUser new_admin = makeUser(QStringLiteral("admin2"), kAllSessions);
    proto::router::UserRequest request = makeRequest(proto::router::kCommandUserAdd, new_admin);

    proto::router::User::WorkspaceKey* key = request.mutable_user()->add_workspace_key();
    key->set_workspace_id(workspace_id);
    key->set_wrapped_gk(toStdString(SealedBox::seal(gk, new_admin.public_key)));

    const UserRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES));

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(db_.findUser(QStringLiteral("admin2")).entry_id,
                                              &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));
}

//--------------------------------------------------------------------------------------------------
// A name the authenticator would never accept is refused before it reaches the database.
TEST_F(UserRequestHandlerTest, AddUserRejectsInvalidName)
{
    RouterUser user = makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT);
    user.name = QStringLiteral("   ");

    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserAdd, user));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// Disabling an account must drop the sessions it opened while it was enabled: the authenticator
// refuses its next login, but a session that is already running would otherwise keep working.
TEST_F(UserRequestHandlerTest, DisablingUserStopsItsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    RouterUser request_user;
    request_user.entry_id = user_id;
    request_user.flags = 0; // Disabled.
    request_user.public_key = db_.findUser(user_id).public_key;

    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserModify, request_user));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.isEmpty());
    EXPECT_EQ(db_.findUser(user_id).flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// A change that revokes nothing must not tear the user's sessions down.
TEST_F(UserRequestHandlerTest, EnabledModifyKeepsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const RouterUser stored = db_.findUser(QStringLiteral("bob"));

    RouterUser request_user;
    request_user.entry_id = stored.entry_id;
    request_user.flags = User::ENABLED;
    request_user.public_key = stored.public_key;

    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserModify, request_user));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES));
}

//--------------------------------------------------------------------------------------------------
// A password rotation invalidates every credential the live sessions authenticated with.
TEST_F(UserRequestHandlerTest, PasswordRotationStopsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    RouterUser rotated = makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT);
    rotated.entry_id = user_id;

    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserModify, rotated));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
}

//--------------------------------------------------------------------------------------------------
// A rejected rotation changed nothing, so the sessions of the user must survive it.
TEST_F(UserRequestHandlerTest, RejectedRotationKeepsSessions)
{
    const SecureByteArray gk(Random::byteArray(32));
    ASSERT_GT(addWorkspace(QStringLiteral("alpha"), gk), 0);

    RouterUser rotated = makeUser(QStringLiteral("admin"), kAllSessions);
    rotated.entry_id = admin_.entry_id;

    // No re-sealed key for the workspace: the whole change is refused.
    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserModify, rotated));

    EXPECT_EQ(result.error_code, proto::router::kErrorConflict);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, ModifyUserRejectsInvalidId)
{
    RouterUser request_user;
    request_user.entry_id = 0;
    request_user.flags = User::ENABLED;

    const UserRequestHandler::Result result =
        handle(makeRequest(proto::router::kCommandUserModify, request_user));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.stop_user_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The deleted account keeps no sessions, and its access entries went with it - both lists are stale.
TEST_F(UserRequestHandlerTest, DeleteUserStopsSessionsAndNotifies)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserDelete, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES));
    EXPECT_FALSE(db_.findUser(user_id).isValid());
}

//--------------------------------------------------------------------------------------------------
// A failed delete (the built-in administrator) must not announce anything or stop any session.
TEST_F(UserRequestHandlerTest, FailedDeleteHasNoSideEffects)
{
    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserDelete, admin_.entry_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_TRUE(db_.findUser(admin_.entry_id).isValid());
}

//--------------------------------------------------------------------------------------------------
// An OTP reset re-enrolls the user, which implies a new device key pair: the tokens issued against
// the old secret die with it and the sessions holding them are dropped.
TEST_F(UserRequestHandlerTest, ResetOtpRevokesTokensAndStopsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    ASSERT_TRUE(db_.setUserOtp(user_id, Random::byteArray(32), 100));
    ASSERT_GT(issueToken(user_id), 0);

    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.isEmpty());
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_TRUE(db_.findUser(user_id).otp_secret.isEmpty());
    EXPECT_EQ(tokenCount(user_id), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, ResetOtpRejectsInvalidUserId)
{
    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, 0));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.stop_user_id, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, ResetOtpOfUnknownUserIsNotFound)
{
    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, 12345));

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// An empty token list means "every token of this user", and every session of the user goes with it.
TEST_F(UserRequestHandlerTest, RevokeAllTokensStopsEverySession)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    ASSERT_GT(issueToken(user_id), 0);
    ASSERT_GT(issueToken(user_id), 0);

    const UserRequestHandler::Result result =
        handle(makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.isEmpty());
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_EQ(tokenCount(user_id), 0u);
}

//--------------------------------------------------------------------------------------------------
// Named tokens: only the sessions holding them are stopped, the rest of the user's sessions stay.
TEST_F(UserRequestHandlerTest, RevokeSelectedTokensStopsOnlyThem)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    const qint64 first_token = issueToken(user_id);
    const qint64 second_token = issueToken(user_id);
    ASSERT_GT(first_token, 0);
    ASSERT_GT(second_token, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(first_token);

    const UserRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_EQ(result.stop_token_ids, QList<qint64>({first_token}));
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// The batch is atomic: a token that is not there rolls the whole revocation back, and nothing is
// stopped for a change that did not happen.
TEST_F(UserRequestHandlerTest, RevokeUnknownTokenIsAtomic)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;

    const qint64 token_id = issueToken(user_id);
    ASSERT_GT(token_id, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(token_id);
    request.mutable_user()->add_token()->set_token_id(token_id + 1000);

    const UserRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// A token of another user must not be revoked through the id of this one.
TEST_F(UserRequestHandlerTest, RevokeForeignTokenIsRejected)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("alice"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);

    const qint64 bob_id = db_.findUser(QStringLiteral("bob")).entry_id;
    const qint64 alice_id = db_.findUser(QStringLiteral("alice")).entry_id;

    const qint64 alice_token = issueToken(alice_id);
    ASSERT_GT(alice_token, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, bob_id);
    request.mutable_user()->add_token()->set_token_id(alice_token);

    const UserRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(tokenCount(alice_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, RevokeTokensRejectsInvalidTokenId)
{
    ASSERT_EQ(db_.addUser(makeUser(QStringLiteral("bob"), proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser(QStringLiteral("bob")).entry_id;
    ASSERT_GT(issueToken(user_id), 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(0);

    const UserRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    const UserRequestHandler::Result result = handle(makeIdRequest("user_frobnicate", 1));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.stop_user_id, 0);
}
