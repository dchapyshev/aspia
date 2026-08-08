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

#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

namespace {

using Result = RequestResult;

//--------------------------------------------------------------------------------------------------
void handleAdd(Database& database, const proto::router::User& user, Result* result)
{
    LOG(INFO) << "User add request:" << user.name();

    RouterUser new_user = RouterUser::parseFrom(user);
    if (!new_user.isValid())
    {
        LOG(ERROR) << "Invalid user record:" << new_user.name;
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    const std::string_view error_code = database.addUser(new_user);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "addUser failed:" << error_code;
        return;
    }

    result->notify_flags = ClientWorker::NOTIFY_USERS;
}

//--------------------------------------------------------------------------------------------------
void handleModify(Database& database, const proto::router::User& user, Result* result)
{
    LOG(INFO) << "User modify request:" << user.name();

    if (user.entry_id() <= 0)
    {
        LOG(ERROR) << "Invalid user ID:" << user.entry_id();
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    RouterUser new_user = RouterUser::parseFrom(user);

    // A request with empty credentials changes only the flags of the record; the stored credentials
    // and the name are kept (see Database::modifyUser), so the fields it does not write are not
    // validated.
    const bool has_credentials = !new_user.salt.isEmpty() || !new_user.verifier.isEmpty();
    if (has_credentials && !new_user.isValid())
    {
        LOG(ERROR) << "Invalid user record:" << new_user.name;
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    // The user update and the token revocation of a password rotation happen in one transaction
    // inside modifyUser, which decides authoritatively whether the rotation happened at all.
    bool password_changed = false;
    const std::string_view error_code = database.modifyUser(new_user, &password_changed);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "modifyUser failed:" << error_code;
        return;
    }

    // Both update paths of modifyUser write the flags of the request, so they are the stored state
    // now. A disabled account must not keep the sessions it opened while it was enabled: the
    // authenticator refuses its next login, but nothing would ever drop a session already running.
    const bool disabled = !(new_user.flags & User::ENABLED);
    if (password_changed || disabled)
        result->stop_user_id = new_user.entry_id;

    result->notify_flags = ClientWorker::NOTIFY_USERS;
}

//--------------------------------------------------------------------------------------------------
void handleDelete(Database& database, const proto::router::User& user, Result* result)
{
    const qint64 entry_id = user.entry_id();

    LOG(INFO) << "User remove request:" << entry_id;

    const std::string_view error_code = database.removeUser(entry_id);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "removeUser failed:" << error_code;
        return;
    }

    result->stop_user_id = entry_id;

    // The cascade dropped the user's access entries and moved the revisions of the affected
    // workspaces, so the cached workspace lists are stale too.
    result->notify_flags = ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES;
}

//--------------------------------------------------------------------------------------------------
void handleResetOtp(Database& database, const RequestCaller& caller, qint64 user_id, Result* result)
{
    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid reset_otp request: user_id=" << user_id;
        result->error_code = proto::router::kErrorInvalidRequest;
        return;
    }

    const std::string_view error_code = database.clearUserOtp(user_id);
    if (error_code != proto::router::kErrorOk)
    {
        result->error_code = error_code;
        return;
    }

    // The secret the live sessions authenticated with is gone, and the user list shows the OTP
    // state - both hold regardless of how the token revocation below ends.
    result->stop_user_id = user_id;
    result->notify_flags = ClientWorker::NOTIFY_USERS;

    // Re-enrollment implies a new device key pair; existing device tokens must die with the
    // secret they were issued against.
    const std::string_view revoke_code = database.revokeUserClientDeviceTokens(user_id);
    if (revoke_code != proto::router::kErrorOk)
    {
        LOG(WARNING) << "OTP cleared but failed to revoke device tokens for user" << user_id
                     << ":" << revoke_code;
        result->error_code = revoke_code;
        return;
    }

    LOG(INFO) << "OTP cleared for user" << user_id << "by" << caller.name;
    result->error_code = proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
void handleRevokeTokens(Database& database, const RequestCaller& caller, const proto::router::User& user,
                        Result* result)
{
    const qint64 user_id = user.entry_id();

    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid revoke_tokens request: user_id=" << user_id;
        result->error_code = proto::router::kErrorInvalidRequest;
        return;
    }

    if (user.token_size() == 0)
    {
        // Empty list - drop every token of the user atomically.
        const std::string_view error_code = database.revokeUserClientDeviceTokens(user_id);
        result->error_code = error_code;

        if (error_code != proto::router::kErrorOk)
            return;

        LOG(INFO) << "All device tokens of user" << user_id << "revoked by" << caller.name;
        result->stop_user_id = user_id;
        result->notify_flags = ClientWorker::NOTIFY_USERS;
        return;
    }

    std::vector<qint64> token_ids;
    token_ids.reserve(user.token_size());

    for (int i = 0; i < user.token_size(); ++i)
    {
        const qint64 token_id = user.token(i).token_id();
        if (token_id <= 0)
        {
            LOG(ERROR) << "Invalid token_id in revoke_tokens request";
            result->error_code = proto::router::kErrorInvalidRequest;
            return;
        }

        token_ids.emplace_back(token_id);
    }

    // Revoke atomically: either every requested token is removed or nothing is, so a missing token
    // or database error never leaves a half-revoked set behind.
    const std::string_view error_code = database.revokeClientDeviceTokens(user_id, token_ids);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
        return;

    LOG(INFO) << token_ids.size() << "device token(s) of user" << user_id
              << "revoked by" << caller.name;
    result->stop_user_id = user_id;
    result->stop_token_ids = std::move(token_ids);
    result->notify_flags = ClientWorker::NOTIFY_USERS;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RequestResult handleUserRequest(Database& database, const RequestCaller& caller,
                                const proto::router::UserRequest& request)
{
    RequestResult result;
    const std::string& command_name = request.command_name();

    if (command_name == proto::router::kCommandUserAdd)
    {
        handleAdd(database, request.user(), &result);
    }
    else if (command_name == proto::router::kCommandUserModify)
    {
        handleModify(database, request.user(), &result);
    }
    else if (command_name == proto::router::kCommandUserDelete)
    {
        handleDelete(database, request.user(), &result);
    }
    else if (command_name == proto::router::kCommandUserResetOtp)
    {
        handleResetOtp(database, caller, request.user().entry_id(), &result);
    }
    else if (command_name == proto::router::kCommandUserRevokeTokens)
    {
        handleRevokeTokens(database, caller, request.user(), &result);
    }
    else
    {
        LOG(ERROR) << "Unknown user request command:" << command_name;
        result.error_code = proto::router::kErrorInvalidRequest;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void handleUserList(Database& database, proto::router::UserList* out)
{
    if (!database.isValid())
    {
        LOG(ERROR) << "Failed to connect to database";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    std::vector<RouterUser> users;
    if (!database.userList(&users))
    {
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->set_error_code(proto::router::kErrorOk);

    for (const auto& user : users)
    {
        proto::router::User* item = out->add_user();
        item->CopyFrom(user.serialize());

        // |otp_active| is a presentation-only flag derived from whether the user has a confirmed
        // TOTP secret on file.
        item->set_otp_active(!user.otp_secret.isEmpty());

        // Attach the user's active device tokens. The router only ever exposes the opaque numeric
        // id and timestamp metadata - never the token hash or any other material that could
        // identify the token outside of the router.
        std::vector<DeviceToken> tokens;
        if (!database.listClientDeviceTokens(user.entry_id, &tokens))
        {
            // A partially built reply must not pass for a complete one.
            out->clear_user();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        for (DeviceToken& src : tokens)
        {
            proto::router::User::Token* token = item->add_token();
            token->set_token_id(src.token_id);
            token->set_created_at(src.created_at);
            token->set_last_used_at(src.last_used_at);
            token->set_address(std::move(src.address));
        }
    }
}

//--------------------------------------------------------------------------------------------------
RequestResult handleChangePassword(Database& database, const RequestCaller& caller,
                                   const proto::router::ChangePasswordRequest& request)
{
    RequestResult result;

    // Checked before the lookup below: a user that could not be read must not pass for a user that
    // is not there - the answers mean different things to the client.
    if (!database.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        result.error_code = proto::router::kErrorInternalError;
        return result;
    }

    // Read-modify-write outside a transaction: the window between this findUser and the modifyUser
    // below is closed only by every users/workspaces write going through the single ClientWorker
    // thread. If client sessions are ever spread over several workers, this must move inside one
    // transaction.
    RouterUser user;
    if (database.findUser(caller.user_id, &user) != proto::router::kErrorOk)
    {
        // The same concurrent delete caught a moment later inside modifyUser answers
        // kErrorNotFound - one event, one code.
        LOG(WARNING) << "Authenticated user not found in database (user_id:" << caller.user_id << ")";
        result.error_code = proto::router::kErrorNotFound;
        return result;
    }

    // Replace only the password-derived fields; keep name, group, sessions, flags intact.
    user.salt             = QByteArray::fromStdString(request.salt());
    user.verifier         = QByteArray::fromStdString(request.verifier());
    user.public_key       = QByteArray::fromStdString(request.public_key());
    user.wrap_private_key = QByteArray::fromStdString(request.wrap_private_key());
    user.wrap_salt        = QByteArray::fromStdString(request.wrap_salt());

    if (!user.isValid())
    {
        LOG(ERROR) << "Rotated credentials produced an invalid user record";
        result.error_code = proto::router::kErrorInvalidData;
        return result;
    }

    // Only the password-derived fields differ here (the rest were loaded from the database), so
    // reusing modifyUser writes back identical values.
    const std::string_view error_code = database.modifyUser(user);
    result.error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Failed to change password for user" << caller.name << ":" << error_code;
        return result;
    }

    // NOTIFY_USERS only: the repair branch of Database::modifyUser cannot create access entries on
    // this path - the keys of the request come from the user's own cryptor cache, which only ever
    // holds the workspaces the user already has an access entry for.
    result.notify_flags = ClientWorker::NOTIFY_USERS;
    return result;
}
