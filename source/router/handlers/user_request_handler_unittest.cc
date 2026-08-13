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
#include "base/crypto/random.h"
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

    // A request that only carries the id (delete, OTP reset).
    proto::router::UserRequest makeIdRequest(std::string_view command, qint64 user_id)
    {
        proto::router::UserRequest request;
        request.set_command_name(std::string(command));
        request.mutable_user()->set_entry_id(user_id);
        return request;
    }

    RequestResult handleTokens(const proto::router::UserTokenRequest& request)
    {
        return handleUserTokenRequest(db_, caller_, request);
    }

    proto::router::UserTokenRequest makeRevokeRequest(qint64 user_id)
    {
        proto::router::UserTokenRequest request;
        request.set_command_name(proto::router::kCommandUserTokenRevoke);
        request.set_user_id(user_id);
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
    EXPECT_TRUE(findUser("bob").isValid());
}

//--------------------------------------------------------------------------------------------------
// An OTP reset re-enrolls the user, which implies a new device key pair: the tokens issued against
// the old secret die with it and the sessions holding them are dropped.
TEST_F(UserRequestHandlerTest, ResetOtpRevokesTokensAndStopsSessions)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = findUser("bob").entry_id;

    ASSERT_TRUE(db_.setUserOtp(user_id, Random::byteArray(32), 100));
    ASSERT_GT(issueToken(user_id), 0);

    const RequestResult result =
        handle(makeIdRequest(proto::router::kCommandUserResetOtp, user_id));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.stop_user_id, user_id);
    EXPECT_TRUE(result.stop_token_ids.empty());
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));
    EXPECT_TRUE(findUser(user_id).otp_secret.isEmpty());
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
    const qint64 user_id = findUser("bob").entry_id;

    ASSERT_GT(issueToken(user_id), 0);
    ASSERT_GT(issueToken(user_id), 0);

    const RequestResult result = handleTokens(makeRevokeRequest(user_id));

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
    const qint64 user_id = findUser("bob").entry_id;

    const qint64 first_token = issueToken(user_id);
    const qint64 second_token = issueToken(user_id);
    ASSERT_GT(first_token, 0);
    ASSERT_GT(second_token, 0);

    proto::router::UserTokenRequest request = makeRevokeRequest(user_id);
    request.add_token_id(first_token);

    const RequestResult result = handleTokens(request);

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
    const qint64 user_id = findUser("bob").entry_id;

    const qint64 token_id = issueToken(user_id);
    ASSERT_GT(token_id, 0);

    proto::router::UserTokenRequest request = makeRevokeRequest(user_id);
    request.add_token_id(token_id);
    request.add_token_id(token_id + 1000);

    const RequestResult result = handleTokens(request);

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

    const qint64 bob_id = findUser("bob").entry_id;
    const qint64 alice_id = findUser("alice").entry_id;

    const qint64 alice_token = issueToken(alice_id);
    ASSERT_GT(alice_token, 0);

    proto::router::UserTokenRequest request = makeRevokeRequest(bob_id);
    request.add_token_id(alice_token);

    const RequestResult result = handleTokens(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.stop_user_id, 0);
    EXPECT_EQ(tokenCount(alice_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, RevokeTokensRejectsInvalidTokenId)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    const qint64 user_id = findUser("bob").entry_id;
    ASSERT_GT(issueToken(user_id), 0);

    proto::router::UserTokenRequest request = makeRevokeRequest(user_id);
    request.add_token_id(0);

    const RequestResult result = handleTokens(request);

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

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, UnknownTokenCommandIsInvalidRequest)
{
    proto::router::UserTokenRequest request = makeRevokeRequest(admin_.entry_id);
    request.set_command_name("token_frobnicate");

    const RequestResult result = handleTokens(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.stop_user_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The tokens of a user are a list of their own, and it carries the opaque metadata alone.
TEST_F(UserRequestHandlerTest, TokenListExposesMetadataOnly)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    proto::router::UserTokenListRequest request;
    request.set_user_id(admin_.entry_id);

    proto::router::UserTokenList list;
    handleUserTokenList(db_, request, &list);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.user_id(), admin_.entry_id);
    ASSERT_EQ(list.token_size(), 1);
    EXPECT_EQ(list.token(0).token_id(), token_id);
    EXPECT_EQ(list.token(0).address(), "127.0.0.1");

    EXPECT_EQ(serialize(list).indexOf(QByteArray::fromStdString(token)), -1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserRequestHandlerTest, TokenListRejectsInvalidUserId)
{
    proto::router::UserTokenListRequest request;
    request.set_user_id(0);

    proto::router::UserTokenList list;
    handleUserTokenList(db_, request, &list);

    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list.token_size(), 0);
}

// The listing of the user records.
class UserListTest : public RouterTestBase
{
protected:
    static proto::router::UserListRequest pageRequest(qint64 offset, qint64 count)
    {
        proto::router::UserListRequest request;
        request.set_offset(offset);
        request.set_count(count);
        return request;
    }

    proto::router::UserList list(const proto::router::UserListRequest& request)
    {
        proto::router::UserList out;
        handleUserList(db_, request, &out);
        return out;
    }
};

//--------------------------------------------------------------------------------------------------
// The page names the records to send, and total_count tells the client how many pages there are.
TEST_F(UserListTest, PageCarriesTotalCount)
{
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(addUser(QString("user%1").arg(i), proto::router::SESSION_TYPE_CLIENT).isValid());

    const proto::router::UserList first = list(pageRequest(0, 2));
    ASSERT_EQ(first.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(first.total_count(), 4);
    ASSERT_EQ(first.user_size(), 2);
    EXPECT_EQ(first.user(0).name(), "admin");

    const proto::router::UserList second = list(pageRequest(2, 2));
    ASSERT_EQ(second.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(second.total_count(), 4);
    EXPECT_EQ(second.user_size(), 2);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserListTest, UnboundedPageIsRefused)
{
    EXPECT_EQ(list(pageRequest(0, 0)).error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list(pageRequest(0, proto::router::kMaxUserPageSize + 1)).error_code(),
              proto::router::kErrorInvalidRequest);
}

//--------------------------------------------------------------------------------------------------
// A lookup answers the single record it names, and needs no page.
TEST_F(UserListTest, LookupAnswersOneRecord)
{
    const RouterUser bob = addUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(bob.isValid());

    proto::router::UserListRequest by_id;
    by_id.set_entry_id(bob.entry_id);

    const proto::router::UserList id_result = list(by_id);
    ASSERT_EQ(id_result.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(id_result.user_size(), 1);
    EXPECT_EQ(id_result.user(0).name(), "bob");
    EXPECT_EQ(id_result.total_count(), 0);

    proto::router::UserListRequest by_name;
    by_name.set_name("bob");

    const proto::router::UserList name_result = list(by_name);
    ASSERT_EQ(name_result.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(name_result.user_size(), 1);
    EXPECT_EQ(name_result.user(0).entry_id(), bob.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The lookup asks whether the record is there, so a miss is an empty list.
TEST_F(UserListTest, LookupMissIsAnEmptyList)
{
    proto::router::UserListRequest request;
    request.set_name("nobody");

    const proto::router::UserList result = list(request);
    EXPECT_EQ(result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(result.user_size(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserListTest, LookupByIdAndNameAtOnceIsRefused)
{
    proto::router::UserListRequest request;
    request.set_entry_id(admin_.entry_id);
    request.set_name("admin");

    EXPECT_EQ(list(request).error_code(), proto::router::kErrorInvalidRequest);
}

//--------------------------------------------------------------------------------------------------
// The credential material of a user stays in the router whatever the listing path.
TEST_F(UserListTest, RecordsCarryNoCredentials)
{
    const proto::router::UserList result = list(pageRequest(0, 10));
    ASSERT_EQ(result.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(result.user_size(), 1);

    const proto::router::User& user = result.user(0);
    EXPECT_EQ(user.entry_id(), admin_.entry_id);
    EXPECT_EQ(user.name(), "admin");
    EXPECT_TRUE(user.salt().empty());
    EXPECT_TRUE(user.verifier().empty());
}

// The rotation of a session's own password over the client channel.
class ChangePasswordTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        caller_.session_type = proto::router::SESSION_TYPE_ADMIN;

        workspace_id_ = addWorkspace("alpha");
        ASSERT_GT(workspace_id_, 0);
    }

    // A rotation request built from a freshly generated set of credentials.
    proto::router::ChangePasswordRequest makeRequest(const RouterUser& rotated)
    {
        proto::router::ChangePasswordRequest request;
        request.set_salt(toStdString(rotated.salt));
        request.set_verifier(toStdString(rotated.verifier));
        return request;
    }

    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// The rotation replaces the credentials and revokes every device token issued against the old
// ones, both in one transaction.
TEST_F(ChangePasswordTest, RotatesCredentialsAndRevokesTokens)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request = makeRequest(rotated);

    const RequestResult result = handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));

    const RouterUser stored = findUser(admin_.entry_id);
    EXPECT_EQ(stored.verifier, rotated.verifier);

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

    ASSERT_EQ(handleChangePassword(db_, caller_, request).error_code, proto::router::kErrorOk);

    const RouterUser stored = findUser(admin_.entry_id);
    EXPECT_EQ(stored.otp_secret, secret);
    EXPECT_EQ(stored.otp_counter, 100u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ChangePasswordTest, RejectsInvalidCredentials)
{
    proto::router::ChangePasswordRequest request; // No credentials at all.

    const RequestResult result = handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(findUser(admin_.entry_id).verifier, admin_.verifier);
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
        { "salt",     &proto::router::ChangePasswordRequest::set_salt },
        { "verifier", &proto::router::ChangePasswordRequest::set_verifier },
    };

    for (const Field& field : fields)
    {
        proto::router::ChangePasswordRequest request = makeRequest(rotated);

        (request.*(field.setter))(oversized);

        const RequestResult result = handleChangePassword(db_, caller_, request);

        EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData) << field.name;
        EXPECT_EQ(findUser(admin_.entry_id).verifier, admin_.verifier) << field.name;
    }
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

    handleChangePassword(db_, caller_, request);

    proto::router::UserListRequest request_list;
    request_list.set_count(proto::router::kMaxUserPageSize);

    proto::router::RouterToAdmin message;
    proto::router::UserList* list = message.mutable_user_list();
    handleUserList(db_, request_list, list);

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

