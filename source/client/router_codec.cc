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

#include "client/router_codec.h"

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "client/router_keys.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr int kGroupKeySize = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
QString decryptField(const DataCryptor& cryptor, std::string_view ciphertext)
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
QByteArray encryptField(const DataCryptor& cryptor, const QString& plaintext)
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
RouterHost decodeRouterHost(const RouterKeys& keys, const proto::router::Host& src)
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

    const DataCryptor* cryptor = keys.workspaceCryptor(src.workspace_id());
    if (!cryptor)
        return dst;

    if (!src.comment().empty())
        dst.comment = decryptField(*cryptor, src.comment());
    if (!src.user_name().empty())
        dst.user_name = decryptField(*cryptor, src.user_name());
    if (!src.password().empty())
        dst.password = SecureString(decryptField(*cryptor, src.password()));

    return dst;
}

//--------------------------------------------------------------------------------------------------
RouterHostList decodeRouterHostList(const RouterKeys& keys, const proto::router::HostList& list)
{
    RouterHostList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = list.workspace_id();
    decoded.group_id     = list.group_id();
    decoded.total_count  = list.total_count();
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
        decoded.hosts.append(decodeRouterHost(keys, list.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList decodeRouterHostSearchResult(const RouterKeys& keys,
                                            const proto::router::HostSearchResult& result)
{
    RouterHostList decoded;
    decoded.error_code = QString::fromStdString(result.error_code());
    decoded.total_count = result.total_count();
    decoded.hosts.reserve(result.host_size());

    for (int i = 0; i < result.host_size(); ++i)
        decoded.hosts.append(decodeRouterHost(keys, result.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList decodeRouterGroupList(const RouterKeys& keys, const proto::router::GroupList& list)
{
    const qint64 workspace_id = list.workspace_id();

    RouterGroupList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = workspace_id;
    decoded.groups.reserve(list.group_size());

    const DataCryptor* cryptor =
        workspace_id != 0 ? keys.workspaceCryptor(workspace_id) : nullptr;

    for (int i = 0; i < list.group_size(); ++i)
    {
        const proto::router::Group& src = list.group(i);

        RouterGroup& dst = decoded.groups.emplaceBack();
        dst.entry_id     = src.entry_id();
        dst.workspace_id = workspace_id;
        dst.parent_id    = src.parent_id();
        dst.name         = QString::fromStdString(src.name());

        if (cryptor && !src.comment().empty())
            dst.comment = decryptField(*cryptor, src.comment());
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterTempHostList decodeRouterTempHostList(const proto::router::TempHostList& list)
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
std::string_view buildRouterWorkspace(const RouterKeys& keys, const RouterWorkspace& workspace,
                                      proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    // Trimmed here because that is the value the router stores and measures.
    out->set_name(workspace.name.trimmed().toStdString());
    out->set_revision(workspace.revision);

    if (!keys.hasPrivateKey())
    {
        LOG(ERROR) << "User private key unavailable";
        return proto::router::kErrorInternalError;
    }

    // A workspace being created has no id yet, so its key is not stored: the entry would live
    // under the id 0 and the next created workspace would find it and get the same group key. The
    // real key comes back sealed for us with the list of the workspaces. An existing workspace
    // without a cached key cannot be modified: a random key would encrypt the comment and the
    // entries of the new users with a key nobody has.
    SecureByteArray group_key;

    if (const DataCryptor* cryptor = keys.workspaceCryptor(workspace.entry_id))
    {
        group_key = cryptor->key();
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
    out->set_comment(encryptField(cryptor, workspace.comment).toStdString());

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
std::string_view buildRouterHost(const RouterKeys& keys, const RouterHost& host,
                                 proto::router::Host* out)
{
    CHECK(out);

    const DataCryptor* cryptor = keys.workspaceCryptor(host.workspace_id);
    if (!cryptor)
    {
        LOG(ERROR) << "No cached cryptor for workspace" << host.workspace_id;
        return proto::router::kErrorInternalError;
    }

    out->set_host_id(host.host_id);
    out->set_group_id(host.group_id);
    out->set_display_name(host.display_name.toStdString());

    out->set_comment(encryptField(*cryptor, host.comment).toStdString());
    out->set_user_name(encryptField(*cryptor, host.user_name).toStdString());
    out->set_password(encryptField(*cryptor, host.password.toString()).toStdString());

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
std::string_view buildRouterGroup(const RouterKeys& keys, qint64 workspace_id,
                                  const RouterGroup& group, proto::router::Group* out)
{
    CHECK(out);

    if (group.entry_id > 0)
        out->set_entry_id(group.entry_id);
    out->set_parent_id(group.parent_id);
    out->set_name(group.name.trimmed().toStdString());

    const DataCryptor* cryptor = keys.workspaceCryptor(workspace_id);
    if (!cryptor)
    {
        LOG(ERROR) << "No cached cryptor for workspace" << workspace_id;
        return proto::router::kErrorInternalError;
    }

    out->set_comment(encryptField(*cryptor, group.comment).toStdString());

    // The name is mandatory.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in group" << group.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}
