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

#include "router/handlers/user_request_handler.h"

#include <vector>

#include "base/serialization.h"
#include "base/net/tcp_channel.h"
#include "proto/router_admin.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The whole user surface of the admin channel, exercised against an isolated database in the state
// --create-config leaves behind: the built-in administrator with id 1.
class UserRequestHandlerTest : public RouterTestBase
{
protected:
    RequestResult handle(const proto::router::UserRequest& request)
    {
        return handleUserRequest(db_, caller_, request);
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
};

//--------------------------------------------------------------------------------------------------
// A user that is not an administrator changes nothing about the workspaces, so only the user list
// is announced as stale.
TEST_F(UserRequestHandlerTest, AddClientNotifiesUsersOnly)
{
    const RequestResult result = handle(makeRequest(
        proto::router::kCommandUserAdd,
        makeUser("bob", proto::router::SESSION_TYPE_CLIENT)));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_TRUE(db_.findUser("bob").isValid());
}

//--------------------------------------------------------------------------------------------------
// A new administrator receives an access entry for every workspace from the keys of the request,
// so the workspace lists of the other sessions become stale as well.
TEST_F(UserRequestHandlerTest, AddAdminGrantsWorkspacesAndNotifies)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", gk);
    ASSERT_GT(workspace_id, 0);

    const RouterUser new_admin = makeUser("admin2", kAllSessions);
    proto::router::UserRequest request = makeRequest(proto::router::kCommandUserAdd, new_admin);

    proto::router::User::WorkspaceKey* key = request.mutable_user()->add_workspace_key();
    key->set_workspace_id(workspace_id);
    key->set_wrapped_gk(toStdString(SealedBox::seal(gk, new_admin.public_key)));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES));

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(db_.findUser("admin2").entry_id,
                                              &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));
}

//--------------------------------------------------------------------------------------------------
// A name the authenticator would never accept is refused before it reaches the database.
TEST_F(UserRequestHandlerTest, AddUserRejectsInvalidName)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    user.name = "   ";

    const RequestResult result =
        handle(makeRequest(proto::router::kCommandUserAdd, user));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// Disabling an account must drop the sessions it opened while it was enabled: the authenticator
// refuses its next login, but a session that is already running would otherwise keep working.
TEST_F(UserRequestHandlerTest, DisablingUserStopsItsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    RouterUser request_user;
    request_user.entry_id = user_id;
    request_user.flags = 0; // Disabled.
    request_user.public_key = db_.findUser(user_id).public_key;

    const RequestResult result =
        handle(makeRequest(proto::router::kCommandUserModify, request_user));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.empty());
    EXPECT_EQ(db_.findUser(user_id).flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// A change that revokes nothing must not tear the user's sessions down.
TEST_F(UserRequestHandlerTest, EnabledModifyKeepsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const RouterUser stored = db_.findUser("bob");

    RouterUser request_user;
    request_user.entry_id = stored.entry_id;
    request_user.flags = User::ENABLED;
    request_user.public_key = stored.public_key;

    const RequestResult result =
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
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    RouterUser rotated = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    rotated.entry_id = user_id;

    const RequestResult result =
        handle(makeRequest(proto::router::kCommandUserModify, rotated));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
}

//--------------------------------------------------------------------------------------------------
// A rejected rotation changed nothing, so the sessions of the user must survive it.
TEST_F(UserRequestHandlerTest, RejectedRotationKeepsSessions)
{
    const SecureByteArray gk(Random::byteArray(32));
    ASSERT_GT(addWorkspace("alpha", gk), 0);

    RouterUser rotated = makeUser("admin", kAllSessions);
    rotated.entry_id = admin_.entry_id;

    // No re-sealed key for the workspace: the whole change is refused.
    const RequestResult result =
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

    const RequestResult result =
        handle(makeRequest(proto::router::kCommandUserModify, request_user));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.stop_user_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The deleted account keeps no sessions, and its access entries went with it - both lists are stale.
TEST_F(UserRequestHandlerTest, DeleteUserStopsSessionsAndNotifies)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    const RequestResult result =
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
    const RequestResult result =
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
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    ASSERT_TRUE(db_.setUserOtp(user_id, Random::byteArray(32), 100));
    ASSERT_GT(issueToken(user_id), 0);

    const RequestResult result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.empty());
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_TRUE(db_.findUser(user_id).otp_secret.isEmpty());
    EXPECT_EQ(tokenCount(user_id), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, ResetOtpRejectsInvalidUserId)
{
    const RequestResult result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, 0));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.stop_user_id, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, ResetOtpOfUnknownUserIsNotFound)
{
    const RequestResult result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, 12345));

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// An empty token list means "every token of this user", and every session of the user goes with it.
TEST_F(UserRequestHandlerTest, RevokeAllTokensStopsEverySession)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    ASSERT_GT(issueToken(user_id), 0);
    ASSERT_GT(issueToken(user_id), 0);

    const RequestResult result =
        handle(makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.empty());
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_EQ(tokenCount(user_id), 0u);
}

//--------------------------------------------------------------------------------------------------
// Named tokens: only the sessions holding them are stopped, the rest of the user's sessions stay.
TEST_F(UserRequestHandlerTest, RevokeSelectedTokensStopsOnlyThem)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    const qint64 first_token = issueToken(user_id);
    const qint64 second_token = issueToken(user_id);
    ASSERT_GT(first_token, 0);
    ASSERT_GT(second_token, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(first_token);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_EQ(result.stop_token_ids, std::vector<qint64>({first_token}));
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// The batch is atomic: a token that is not there rolls the whole revocation back, and nothing is
// stopped for a change that did not happen.
TEST_F(UserRequestHandlerTest, RevokeUnknownTokenIsAtomic)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    const qint64 token_id = issueToken(user_id);
    ASSERT_GT(token_id, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(token_id);
    request.mutable_user()->add_token()->set_token_id(token_id + 1000);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// A token of another user must not be revoked through the id of this one.
TEST_F(UserRequestHandlerTest, RevokeForeignTokenIsRejected)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    ASSERT_EQ(db_.addUser(makeUser("alice", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);

    const qint64 bob_id = db_.findUser("bob").entry_id;
    const qint64 alice_id = db_.findUser("alice").entry_id;

    const qint64 alice_token = issueToken(alice_id);
    ASSERT_GT(alice_token, 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, bob_id);
    request.mutable_user()->add_token()->set_token_id(alice_token);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(tokenCount(alice_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, RevokeTokensRejectsInvalidTokenId)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;
    ASSERT_GT(issueToken(user_id), 0);

    proto::router::UserRequest request =
        makeIdRequest(proto::router::kCommandUserRevokeTokens, user_id);
    request.mutable_user()->add_token()->set_token_id(0);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(tokenCount(user_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    const RequestResult result = handle(makeIdRequest("user_frobnicate", 1));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.stop_user_id, 0);
}

// The rotation of a session's own password over the client channel.
class ChangePasswordTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        caller_.session_type = proto::router::SESSION_TYPE_ADMIN;

        gk_ = SecureByteArray(Random::byteArray(32));
        workspace_id_ = addWorkspace("alpha", gk_);
        ASSERT_GT(workspace_id_, 0);
    }

    // A rotation request built from a freshly generated set of credentials.
    proto::router::ChangePasswordRequest makeRequest(const RouterUser& rotated)
    {
        proto::router::ChangePasswordRequest request;
        request.set_salt(toStdString(rotated.salt));
        request.set_verifier(toStdString(rotated.verifier));
        request.set_public_key(toStdString(rotated.public_key));
        request.set_wrap_private_key(toStdString(rotated.wrap_private_key));
        request.set_wrap_salt(toStdString(rotated.wrap_salt));
        return request;
    }

    void addWorkspaceKey(proto::router::ChangePasswordRequest* request, const RouterUser& rotated)
    {
        proto::router::ChangePasswordRequest::WorkspaceKey* key = request->add_workspace_key();
        key->set_workspace_id(workspace_id_);
        key->set_wrapped_gk(toStdString(SealedBox::seal(gk_, rotated.public_key)));
    }

    SecureByteArray gk_;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// The rotation replaces the credentials, revokes every device token issued against the old ones
// and re-wraps the workspace keys - all in one transaction.
TEST_F(ChangePasswordTest, RotatesCredentialsAndRevokesTokens)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request = makeRequest(rotated);
    addWorkspaceKey(&request, rotated);

    const RequestResult result = handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));

    const RouterUser stored = db_.findUser(admin_.entry_id);
    EXPECT_EQ(stored.verifier, rotated.verifier);
    EXPECT_EQ(stored.public_key, rotated.public_key);

    // The name, the session mask and the flags of the record are not the caller's to change here.
    EXPECT_EQ(stored.name, admin_.name);
    EXPECT_EQ(stored.sessions, admin_.sessions);
    EXPECT_EQ(stored.flags, admin_.flags);

    std::vector<DeviceToken> tokens;
    ASSERT_TRUE(db_.listClientDeviceTokens(admin_.entry_id, &tokens));
    EXPECT_TRUE(tokens.empty());
}

//--------------------------------------------------------------------------------------------------
// The second factor is not derived from the password: a rotation must leave the enrollment alone,
// otherwise every password change would silently drop the user back to a fresh TOTP enrollment.
TEST_F(ChangePasswordTest, KeepsOtpEnrollment)
{
    const QByteArray secret = Random::byteArray(32);
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 100));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request = makeRequest(rotated);
    addWorkspaceKey(&request, rotated);

    ASSERT_EQ(handleChangePassword(db_, caller_, request).error_code, proto::router::kErrorOk);

    const RouterUser stored = db_.findUser(admin_.entry_id);
    EXPECT_EQ(stored.otp_secret, secret);
    EXPECT_EQ(stored.otp_counter, 100u);
}

//--------------------------------------------------------------------------------------------------
// Without a re-sealed key for every workspace the user can access the rotation would lock it out
// of them, so nothing is applied - the credentials and the tokens survive intact.
TEST_F(ChangePasswordTest, WithoutKeysIsConflict)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    const RequestResult result =
        handleChangePassword(db_, caller_, makeRequest(makeUser("admin", kAllSessions)));

    EXPECT_EQ(result.error_code, proto::router::kErrorConflict);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);

    std::vector<DeviceToken> tokens;
    ASSERT_TRUE(db_.listClientDeviceTokens(admin_.entry_id, &tokens));
    EXPECT_EQ(tokens.size(), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ChangePasswordTest, RejectsInvalidCredentials)
{
    proto::router::ChangePasswordRequest request; // No credentials at all.

    const RequestResult result = handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);
}

//--------------------------------------------------------------------------------------------------
// Every blob here is a fixed-size product of the crypto, so anything larger is not a credential.
TEST_F(ChangePasswordTest, RejectsOversizedCredentials)
{
    const RouterUser rotated = makeUser("admin", kAllSessions);
    const std::string oversized(64 * 1024, 'x');

    struct Field
    {
        const char* name;
        void (proto::router::ChangePasswordRequest::*setter)(const std::string&);
    };

    const Field fields[] =
    {
        { "salt",             &proto::router::ChangePasswordRequest::set_salt },
        { "verifier",         &proto::router::ChangePasswordRequest::set_verifier },
        { "public_key",       &proto::router::ChangePasswordRequest::set_public_key },
        { "wrap_private_key", &proto::router::ChangePasswordRequest::set_wrap_private_key },
        { "wrap_salt",        &proto::router::ChangePasswordRequest::set_wrap_salt },
    };

    for (const Field& field : fields)
    {
        proto::router::ChangePasswordRequest request = makeRequest(rotated);
        addWorkspaceKey(&request, rotated);

        (request.*(field.setter))(oversized);

        const RequestResult result = handleChangePassword(db_, caller_, request);

        EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData) << field.name;
        EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier) << field.name;
    }
}

//--------------------------------------------------------------------------------------------------
// The router stores the re-sealed key without looking inside, so its size is all it can judge.
TEST_F(ChangePasswordTest, RejectsAnOversizedWorkspaceKey)
{
    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request = makeRequest(rotated);

    proto::router::ChangePasswordRequest::WorkspaceKey* key = request.add_workspace_key();
    key->set_workspace_id(workspace_id_);
    key->set_wrapped_gk(std::string(64 * 1024, 'x'));

    const RequestResult result = handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);
}

//--------------------------------------------------------------------------------------------------
// A client rotates its own credentials unattended, and they all land in the administrator user
// list. An unbounded record would kill every administrator session that opens that list,
// including the one that would delete the offender.
TEST_F(ChangePasswordTest, UserListStaysSendableAfterACredentialRotation)
{
    const RouterUser client = addUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const std::string oversized(1024 * 1024, 'x');

    proto::router::ChangePasswordRequest request;
    request.set_salt(oversized);
    request.set_verifier(oversized);
    request.set_public_key(oversized);
    request.set_wrap_private_key(oversized);
    request.set_wrap_salt(oversized);

    handleChangePassword(db_, caller_, request);

    proto::router::RouterToAdmin message;
    proto::router::UserList* list = message.mutable_user_list();
    handleUserList(db_, list);

    ASSERT_EQ(list->error_code(), proto::router::kErrorOk);
    EXPECT_LE(serialize(message).size(), TcpChannel::kMaxMessageSize);
}

//--------------------------------------------------------------------------------------------------
// The account was deleted while its session was live: the answer is the same one the modify path
// gives a moment later.
TEST_F(ChangePasswordTest, OfDeletedUserIsNotFound)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    const RouterUser rotated = makeUser("client",
                                        proto::router::SESSION_TYPE_CLIENT);

    const RequestResult result = handleChangePassword(db_, caller_, makeRequest(rotated));

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// The user list of the admin channel carries the active device tokens of every user - only their
// opaque metadata, never the token material itself.
TEST_F(ChangePasswordTest, UserListExposesTokenMetadataOnly)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    proto::router::UserList list;
    handleUserList(db_, &list);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.user_size(), 1);
    ASSERT_EQ(list.user(0).token_size(), 1);

    const proto::router::User::Token& stored = list.user(0).token(0);
    EXPECT_EQ(stored.token_id(), token_id);
    EXPECT_EQ(stored.address(), "127.0.0.1");

    // The token itself must not surface anywhere in the reply.
    EXPECT_EQ(serialize(list).indexOf(QByteArray::fromStdString(token)), -1);
}
