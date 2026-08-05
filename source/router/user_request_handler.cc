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

#include <unordered_map>

#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

namespace {

using Result = UserRequestHandler::Result;

//--------------------------------------------------------------------------------------------------
// The group keys the sender re-sealed to the key pair of the user. The router can neither unseal
// nor reseal them, it only stores them as the access entries of the user.
std::unordered_map<qint64, QByteArray> workspaceKeys(const proto::router::User& user)
{
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.reserve(user.workspace_key_size());

    for (int i = 0; i < user.workspace_key_size(); ++i)
    {
        const proto::router::User::WorkspaceKey& wk = user.workspace_key(i);
        wrapped_keys.emplace(wk.workspace_id(), QByteArray::fromStdString(wk.wrapped_gk()));
    }

    return wrapped_keys;
}

//--------------------------------------------------------------------------------------------------
void handleAdd(Database& database, const RequestCaller& caller, const proto::router::User& user,
               Result* result)
{
    LOG(INFO) << "User add request:" << user.name();

    RouterUser new_user = RouterUser::parseFrom(user);
    if (!new_user.isValid())
    {
        LOG(ERROR) << "Invalid user record:" << new_user.name;
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    // An administrator has access to every workspace, so the keys of the request become the access
    // entries of the new user (see Database::addUser).
    const std::string_view error_code =
        database.addUser(new_user, workspaceKeys(user), caller.user_id);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "addUser failed:" << error_code;
        return;
    }

    // An administrator could have received access entries for the workspaces it had none for, so
    // the clients must refetch the list of the workspaces as well.
    result->notify_flags = ClientWorker::NOTIFY_USERS;
    if (new_user.sessions & proto::router::SESSION_TYPE_ADMIN)
        result->notify_flags |= ClientWorker::NOTIFY_WORKSPACES;
}

//--------------------------------------------------------------------------------------------------
void handleModify(Database& database, const RequestCaller& caller, const proto::router::User& user,
                  Result* result)
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

    // On a password rotation the stored wrapped GKs must be replaced with the keys re-sealed by
    // the admin to the new key pair. The user update, token revocation and re-wrap happen in one
    // transaction inside modifyUser; if the re-sealed set is incomplete the whole change is
    // rejected, so the user never loses workspace access. The keys are only consumed when the
    // password actually changes (decided authoritatively inside modifyUser).
    bool password_changed = false;
    const std::string_view error_code =
        database.modifyUser(new_user, workspaceKeys(user), caller.user_id, &password_changed);
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

    // The access level of the request is not authoritative, so whether access entries were created
    // for the workspaces (see Database::modifyUser) is unknown here. The list of the workspaces is
    // refetched in any case: a user is modified rarely.
    result->notify_flags = ClientWorker::NOTIFY_USERS | ClientWorker::NOTIFY_WORKSPACES;
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

    QList<qint64> token_ids;
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

        token_ids.append(token_id);
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
// static
UserRequestHandler::Result UserRequestHandler::handle(
    Database& database, const RequestCaller& caller, const proto::router::UserRequest& request)
{
    Result result;
    const std::string& command_name = request.command_name();

    if (command_name == proto::router::kCommandUserAdd)
    {
        handleAdd(database, caller, request.user(), &result);
    }
    else if (command_name == proto::router::kCommandUserModify)
    {
        handleModify(database, caller, request.user(), &result);
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
