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

#include "client/router_state.h"

#include <set>

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr int kGroupKeySize = 32;

//--------------------------------------------------------------------------------------------------
QByteArray encrypt(const DataCryptor& cryptor, const QString& plaintext)
{
    if (plaintext.isEmpty())
        return QByteArray();

    std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext.toUtf8());
    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Failed to encrypt with group key";
        return QByteArray();
    }

    return *encrypted;
}

//--------------------------------------------------------------------------------------------------
QString decrypt(const DataCryptor& cryptor, std::string_view ciphertext)
{
    if (ciphertext.empty())
        return QString();

    std::optional<QByteArray> decrypted = cryptor.decrypt(
        QByteArrayView(ciphertext.data(), static_cast<qsizetype>(ciphertext.size())));
    if (!decrypted.has_value())
    {
        LOG(ERROR) << "Failed to decrypt with group key";
        return QString();
    }

    return QString::fromUtf8(*decrypted);
}

//--------------------------------------------------------------------------------------------------
// Re-seal every workspace key in |cryptors| to |new_public_key| and append the results to
// |message| (any proto with a repeated WorkspaceKey workspace_key field).
template<typename MessageT>
void resealWorkspaceKeys(const std::unordered_map<qint64, DataCryptor>& cryptors,
                         const QByteArray& new_public_key, MessageT* message)
{
    for (const auto& [workspace_id, cryptor] : cryptors)
    {
        QByteArray wrapped_gk = SealedBox::seal(cryptor.key(), new_public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to reseal group key for workspace" << workspace_id;
            continue;
        }

        auto* dst = message->add_workspace_key();
        dst->set_workspace_id(workspace_id);
        dst->set_wrapped_gk(wrapped_gk.toStdString());
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterState::KeysResult RouterState::applyUserKeys(const proto::router::UserKeys& user_keys,
                                                   const SecureString& password)
{
    user_id_   = user_keys.user_id();
    user_name_ = QString::fromStdString(user_keys.name());

    const QByteArray wrap_private_key = QByteArray::fromStdString(user_keys.wrap_private_key());
    const QByteArray wrap_salt = QByteArray::fromStdString(user_keys.wrap_salt());

    if (wrap_private_key.isEmpty() || wrap_salt.isEmpty())
    {
        user_private_key_.clear();
        return KeysResult::PASSWORD_CHANGE_REQUIRED;
    }

    user_private_key_ = PrivateKeyCryptor::decrypt(wrap_private_key, password, wrap_salt);
    if (user_private_key_.isEmpty())
    {
        LOG(WARNING) << "Failed to decrypt self private key for user_id:" << user_id_;
        return KeysResult::DECRYPT_FAILED;
    }

    workspace_cryptors_.clear();
    for (int i = 0; i < user_keys.workspace_key_size(); ++i)
    {
        const proto::router::UserKeys::WorkspaceKey& wk = user_keys.workspace_key(i);
        SecureByteArray gk = unwrapGroupKey(QByteArray::fromStdString(wk.wrapped_gk()));
        if (gk.isEmpty())
        {
            // The hole this leaves makes every reseal-dependent operation (own password change,
            // creating an administrator) answer "conflict" for this workspace, and no refetch can
            // repair it - only a re-grant can.
            LOG(ERROR) << "Failed to unwrap GK for workspace" << wk.workspace_id();
            continue;
        }

        workspace_cryptors_.emplace(wk.workspace_id(),
                                    DataCryptor(CipherType::AES256_GCM, std::move(gk)));
    }

    return KeysResult::OK;
}

//--------------------------------------------------------------------------------------------------
void RouterState::clearSession()
{
    user_id_ = 0;
    user_name_.clear();
    user_private_key_.clear();
    workspace_cryptors_.clear();
    pending_.clear();
}

//--------------------------------------------------------------------------------------------------
void RouterState::clearCaches()
{
    workspaces_loaded_ = false;
    cached_workspaces_ = RouterWorkspaceList();
    cached_groups_.clear();
    cached_hosts_.clear();
}

//--------------------------------------------------------------------------------------------------
bool RouterState::hasWorkspaceKey(qint64 workspace_id) const
{
    return workspace_cryptors_.find(workspace_id) != workspace_cryptors_.end();
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToAdmin& message)
{
    if (message.has_relay_list())
    {
        dispatch(message.relay_list().request_id(), message.relay_list());
    }
    else if (message.has_client_list())
    {
        dispatch(message.client_list().request_id(), message.client_list());
    }
    else if (message.has_user_list())
    {
        dispatch(message.user_list().request_id(), message.user_list());
    }
    else if (message.has_user_result())
    {
        const proto::router::UserResult& result = message.user_result();
        const std::string& command = result.command_name();

        // Adding an administrator grants it an access entry in every workspace and deleting a user
        // drops its entries by cascade; both move the revisions of the workspaces involved.
        // reset_otp and revoke_tokens touch nothing but the user itself.
        const bool moves_workspaces = command == proto::router::kCommandUserAdd ||
                                      command == proto::router::kCommandUserModify ||
                                      command == proto::router::kCommandUserDelete;

        if (moves_workspaces && result.error_code() == proto::router::kErrorOk)
            invalidateWorkspaces();

        dispatch(result.request_id(), result);
    }
    else if (message.has_host_result())
    {
        if (message.host_result().error_code() == proto::router::kErrorOk)
            clearHostCache();

        dispatch(message.host_result().request_id(), message.host_result());
    }
    else if (message.has_relay_result())
    {
        dispatch(message.relay_result().request_id(), message.relay_result());
    }
    else if (message.has_client_result())
    {
        dispatch(message.client_result().request_id(), message.client_result());
    }
    else if (message.has_workspace_result())
    {
        const proto::router::WorkspaceResult& result = message.workspace_result();
        if (result.error_code() == proto::router::kErrorOk)
        {
            // Every workspace operation assigns hosts to the workspace or releases them from it,
            // and deleting one takes its whole group tree with it - the same three lists the
            // router marks as changed.
            invalidateWorkspaces();
            clearHostCache();

            if (result.command_name() == proto::router::kCommandWorkspaceDelete)
                clearGroupCache();
        }

        dispatch(result.request_id(), result);
    }
    else if (message.has_peer_result())
    {
        dispatch(message.peer_result().request_id(), message.peer_result());
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToManager& message)
{
    if (message.has_host_result())
    {
        if (message.host_result().error_code() == proto::router::kErrorOk)
            clearHostCache();

        dispatch(message.host_result().request_id(), message.host_result());
    }
    else if (message.has_group_result())
    {
        const proto::router::GroupResult& result = message.group_result();
        if (result.error_code() == proto::router::kErrorOk)
        {
            // The result carries no workspace id, so the whole group cache goes; group edits are
            // rare enough that the extra reload does not matter. A deleted group also releases its
            // hosts, which is why the host lists go with it.
            clearGroupCache();

            if (result.command_name() == proto::router::kCommandGroupDelete)
                clearHostCache();
        }

        dispatch(result.request_id(), result);
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToClient& message)
{
    // Nothing here invalidates a cache: these are read-only queries, and the one write among them
    // (the password change) re-seals the keys the user already holds without moving any revision.
    if (message.has_connection_offer())
        dispatch(message.connection_offer().request_id(), message.connection_offer());
    else if (message.has_host_status())
        dispatch(message.host_status().request_id(), message.host_status());
    else if (message.has_host_list())
        dispatch(message.host_list().request_id(), message.host_list());
    else if (message.has_host_search_result())
        dispatch(message.host_search_result().request_id(), message.host_search_result());
    else if (message.has_temp_host_list())
        dispatch(message.temp_host_list().request_id(), message.temp_host_list());
    else if (message.has_workspace_list())
        dispatch(message.workspace_list().request_id(), message.workspace_list());
    else if (message.has_group_list())
        dispatch(message.group_list().request_id(), message.group_list());
    else if (message.has_change_password_result())
        dispatch(message.change_password_result().request_id(), message.change_password_result());
    else
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList RouterState::applyWorkspaceList(const proto::router::WorkspaceList& list,
                                                    qint64 requested_workspace_id)
{
    RouterWorkspaceList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.workspaces.reserve(list.workspace_size());

    // The complete list is the authoritative answer about what we still have access to.
    const bool full_list = requested_workspace_id == 0 &&
                           decoded.error_code == proto::router::kErrorOk;
    std::set<qint64> visible_ids;

    for (int i = 0; i < list.workspace_size(); ++i)
    {
        const proto::router::Workspace& src = list.workspace(i);

        RouterWorkspace& dst = decoded.workspaces.emplaceBack();
        dst.entry_id = src.entry_id();
        dst.name     = QString::fromStdString(src.name());
        dst.revision = src.revision();
        dst.access.reserve(src.access_size());

        visible_ids.insert(src.entry_id());

        QByteArray self_wrapped_gk;
        for (int j = 0; j < src.access_size(); ++j)
        {
            const proto::router::WorkspaceAccess& access = src.access(j);
            dst.access.emplaceBack().user_id = access.user_id();

            if (access.user_id() == user_id_)
                self_wrapped_gk = QByteArray::fromStdString(access.wrapped_gk());
        }

        if (self_wrapped_gk.isEmpty())
            continue;

        SecureByteArray gk = unwrapGroupKey(self_wrapped_gk);
        if (gk.isEmpty())
        {
            // See applyUserKeys: the missing cryptor makes reseal-dependent operations answer
            // "conflict" for this workspace with no way for a refetch to recover.
            LOG(ERROR) << "Failed to unwrap GK for workspace" << src.entry_id();
            continue;
        }

        DataCryptor cryptor(CipherType::AES256_GCM, gk);
        if (!src.comment().empty())
            dst.comment = decrypt(cryptor, src.comment());

        workspace_cryptors_.insert_or_assign(src.entry_id(), std::move(cryptor));
    }

    if (full_list)
    {
        // Access to a workspace can be revoked, and the workspace itself can be deleted. Keeping
        // its group key would let us keep decrypting its records and, worse, hand the key to a
        // user we grant access to - long after we lost it ourselves.
        std::erase_if(workspace_cryptors_, [&visible_ids](const auto& item)
        {
            return !visible_ids.contains(item.first);
        });

        cached_workspaces_ = decoded;
        workspaces_loaded_ = true;
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList RouterState::applyHostList(const proto::router::HostList& list,
                                          const HostCacheKey& key, bool cacheable)
{
    RouterHostList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = list.workspace_id();
    decoded.group_id     = list.group_id();
    decoded.total_count  = list.total_count();
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
        decoded.hosts.append(decodeHost(list.host(i)));

    // An error reply carries no list - caching it would serve the emptiness as a success.
    if (cacheable && decoded.error_code == proto::router::kErrorOk)
        cached_hosts_[key] = decoded;

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList RouterState::applyGroupList(const proto::router::GroupList& list)
{
    const qint64 workspace_id = list.workspace_id();

    RouterGroupList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = workspace_id;
    decoded.groups.reserve(list.group_size());

    const auto it = workspace_cryptors_.find(workspace_id);
    const bool has_key = workspace_id != 0 && it != workspace_cryptors_.end();

    for (int i = 0; i < list.group_size(); ++i)
    {
        const proto::router::Group& src = list.group(i);

        RouterGroup& dst = decoded.groups.emplaceBack();
        dst.entry_id     = src.entry_id();
        dst.workspace_id = workspace_id;
        dst.parent_id    = src.parent_id();
        dst.name         = QString::fromStdString(src.name());

        if (has_key && !src.comment().empty())
            dst.comment = decrypt(it->second, src.comment());
    }

    if (decoded.error_code == proto::router::kErrorOk)
        cached_groups_[workspace_id] = decoded;

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList RouterState::decodeHostSearchResult(
    const proto::router::HostSearchResult& result) const
{
    RouterHostList decoded;
    decoded.error_code = QString::fromStdString(result.error_code());
    decoded.total_count = result.total_count();
    decoded.hosts.reserve(result.host_size());

    for (int i = 0; i < result.host_size(); ++i)
        decoded.hosts.append(decodeHost(result.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterTempHostList RouterState::decodeTempHostList(const proto::router::TempHostList& list) const
{
    RouterTempHostList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
    {
        const proto::router::TempHost& src = list.host(i);

        RouterTempHost& dst = decoded.hosts.emplaceBack();
        dst.temp_id       = src.temp_id();
        dst.computer_name = QString::fromStdString(src.computer_name());
        dst.version       = QString::fromStdString(src.version());
        dst.os_name       = QString::fromStdString(src.os_name());
        dst.address       = QString::fromStdString(src.address());
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList RouterState::cachedWorkspaceList() const
{
    RouterWorkspaceList cached = cached_workspaces_;
    cached.error_code = proto::router::kErrorOk;
    return cached;
}

//--------------------------------------------------------------------------------------------------
const RouterHostList* RouterState::cachedHostList(const HostCacheKey& key) const
{
    const auto it = cached_hosts_.constFind(key);
    if (it == cached_hosts_.constEnd())
        return nullptr;
    return &it.value();
}

//--------------------------------------------------------------------------------------------------
const RouterGroupList* RouterState::cachedGroupList(qint64 workspace_id) const
{
    const auto it = cached_groups_.constFind(workspace_id);
    if (it == cached_groups_.constEnd())
        return nullptr;
    return &it.value();
}

//--------------------------------------------------------------------------------------------------
std::string_view RouterState::buildWorkspace(const RouterWorkspace& workspace,
                                             proto::router::Workspace* out) const
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    // Trimmed here because that is the value the router stores and measures.
    out->set_name(workspace.name.trimmed().toStdString());
    out->set_revision(workspace.revision);

    if (user_private_key_.isEmpty())
    {
        LOG(ERROR) << "User private key unavailable";
        return proto::router::kErrorInternalError;
    }

    // A workspace being created has no id yet, so its key is not put into |workspace_cryptors_|:
    // the entry would be stored under the id 0 and the next created workspace would find it and
    // get the same group key. The real key comes back sealed for us with the list of the
    // workspaces. An existing workspace without a cached key cannot be modified: a random key
    // would encrypt the comment and the entries of the new users with a key nobody has.
    SecureByteArray group_key;

    const auto it = workspace_cryptors_.find(workspace.entry_id);
    if (it != workspace_cryptors_.end())
    {
        group_key = it->second.key();
    }
    else if (workspace.entry_id == 0)
    {
        group_key = SecureByteArray(Random::byteArray(kGroupKeySize));
    }
    else
    {
        LOG(ERROR) << "No group key for workspace" << workspace.entry_id;
        return proto::router::kErrorInternalError;
    }

    const DataCryptor cryptor(CipherType::AES256_GCM, group_key);
    out->set_comment(encrypt(cryptor, workspace.comment).toStdString());

    for (const auto& access : workspace.access)
    {
        proto::router::WorkspaceAccess* dst = out->add_access();
        dst->set_user_id(access.user_id);

        if (access.public_key.isEmpty())
            continue; // Existing user - server preserves their wrapped_gk.

        QByteArray wrapped_gk = SealedBox::seal(group_key, access.public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to seal group key for user_id:" << access.user_id;
            return proto::router::kErrorInternalError;
        }
        dst->set_wrapped_gk(wrapped_gk.toStdString());
        // The seal target travels with the key: the router checks it against the stored key of
        // the user and rejects an entry sealed to an out of date snapshot.
        dst->set_public_key(access.public_key.toStdString());

        if (dst->wrapped_gk().size() > proto::router::kMaxWrappedKeyLength)
        {
            LOG(ERROR) << "Oversized wrapped key for user_id:" << access.user_id;
            return proto::router::kErrorInvalidData;
        }
    }

    for (HostId host_id : std::as_const(workspace.host_ids))
        out->add_host_id(host_id);

    // The name is mandatory. Sizes are of the bytes that go out, not of the text the user typed.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in workspace" << workspace.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view RouterState::buildHost(const RouterHost& host, proto::router::Host* out) const
{
    CHECK(out);

    const auto it = workspace_cryptors_.find(host.workspace_id);
    if (it == workspace_cryptors_.end())
    {
        LOG(ERROR) << "No cached cryptor for workspace" << host.workspace_id;
        return proto::router::kErrorInternalError;
    }

    out->set_host_id(host.host_id);
    out->set_group_id(host.group_id);
    out->set_display_name(host.display_name.toStdString());

    const DataCryptor& cryptor = it->second;
    out->set_comment(encrypt(cryptor, host.comment).toStdString());
    out->set_user_name(encrypt(cryptor, host.user_name).toStdString());
    out->set_password(encrypt(cryptor, host.password.toString()).toStdString());

    // Every field of a host is optional (an empty display name falls back to the computer name),
    // so only the sizes are checked.
    if (out->display_name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength ||
        out->user_name().size() > proto::router::kMaxCredentialLength ||
        out->password().size() > proto::router::kMaxCredentialLength)
    {
        LOG(ERROR) << "Oversized field in host" << host.host_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view RouterState::buildGroup(qint64 workspace_id, const RouterGroup& group,
                                         proto::router::Group* out) const
{
    CHECK(out);

    if (group.entry_id > 0)
        out->set_entry_id(group.entry_id);
    out->set_parent_id(group.parent_id);
    out->set_name(group.name.trimmed().toStdString());

    const auto it = workspace_cryptors_.find(workspace_id);
    if (it == workspace_cryptors_.end())
    {
        LOG(ERROR) << "No cached cryptor for workspace" << workspace_id;
        return proto::router::kErrorInternalError;
    }

    out->set_comment(encrypt(it->second, group.comment).toStdString());

    // The name is mandatory.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in group" << group.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
void RouterState::resealGroupKeys(const QByteArray& new_public_key,
                                  proto::router::User* user) const
{
    CHECK(user);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, user);
}

//--------------------------------------------------------------------------------------------------
void RouterState::resealGroupKeys(const QByteArray& new_public_key,
                                  proto::router::ChangePasswordRequest* request) const
{
    CHECK(request);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, request);
}

//--------------------------------------------------------------------------------------------------
RouterHost RouterState::decodeHost(const proto::router::Host& src) const
{
    RouterHost dst;
    dst.host_id       = src.host_id();
    dst.workspace_id  = src.workspace_id();
    dst.group_id      = src.group_id();
    dst.display_name  = QString::fromStdString(src.display_name());
    dst.computer_name = QString::fromStdString(src.computer_name());
    dst.cpu_arch      = QString::fromStdString(src.cpu_arch());
    dst.version       = QString::fromStdString(src.version());
    dst.os_name       = QString::fromStdString(src.os_name());
    dst.address       = QString::fromStdString(src.address());
    dst.last_connect  = src.last_connect();
    dst.last_modify   = src.last_modify();
    dst.online        = src.online();

    if (src.workspace_id() == 0)
        return dst;

    const auto it = workspace_cryptors_.find(src.workspace_id());
    if (it == workspace_cryptors_.end())
        return dst;

    const DataCryptor& cryptor = it->second;
    if (!src.comment().empty())
        dst.comment = decrypt(cryptor, src.comment());
    if (!src.user_name().empty())
        dst.user_name = decrypt(cryptor, src.user_name());
    if (!src.password().empty())
        dst.password = SecureString(decrypt(cryptor, src.password()));

    return dst;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray RouterState::unwrapGroupKey(const QByteArray& wrapped_gk) const
{
    if (wrapped_gk.isEmpty() || user_private_key_.isEmpty())
        return SecureByteArray();

    KeyPair key_pair = KeyPair::fromPrivateKey(user_private_key_);
    if (!key_pair.isValid())
    {
        LOG(ERROR) << "Failed to load key pair from private key";
        return SecureByteArray();
    }

    std::optional<SecureByteArray> opened = SealedBox::open(wrapped_gk, key_pair);
    if (!opened.has_value() || opened->isEmpty())
    {
        LOG(ERROR) << "Failed to open sealed group key";
        return SecureByteArray();
    }

    return std::move(*opened);
}
