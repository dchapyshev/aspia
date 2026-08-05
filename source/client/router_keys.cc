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

#include "client/router_keys.h"

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/sealed_box.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"

namespace {

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
RouterKeys::Result RouterKeys::apply(const proto::router::UserKeys& user_keys,
                                     const SecureString& password)
{
    user_id_   = user_keys.user_id();
    user_name_ = QString::fromStdString(user_keys.name());

    const QByteArray wrap_private_key = QByteArray::fromStdString(user_keys.wrap_private_key());
    const QByteArray wrap_salt = QByteArray::fromStdString(user_keys.wrap_salt());

    if (wrap_private_key.isEmpty() || wrap_salt.isEmpty())
    {
        user_private_key_.clear();
        return Result::PASSWORD_CHANGE_REQUIRED;
    }

    user_private_key_ = PrivateKeyCryptor::decrypt(wrap_private_key, password, wrap_salt);
    if (user_private_key_.isEmpty())
    {
        LOG(WARNING) << "Failed to decrypt self private key for user_id:" << user_id_;
        return Result::DECRYPT_FAILED;
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

    return Result::OK;
}

//--------------------------------------------------------------------------------------------------
void RouterKeys::clear()
{
    user_id_ = 0;
    user_name_.clear();
    user_private_key_.clear();
    workspace_cryptors_.clear();
}

//--------------------------------------------------------------------------------------------------
bool RouterKeys::hasWorkspaceKey(qint64 workspace_id) const
{
    return workspaceCryptor(workspace_id) != nullptr;
}

//--------------------------------------------------------------------------------------------------
const DataCryptor* RouterKeys::workspaceCryptor(qint64 workspace_id) const
{
    const auto it = workspace_cryptors_.find(workspace_id);
    if (it == workspace_cryptors_.end())
        return nullptr;
    return &it->second;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray RouterKeys::unwrapGroupKey(const QByteArray& wrapped_gk) const
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

//--------------------------------------------------------------------------------------------------
void RouterKeys::storeWorkspaceKey(qint64 workspace_id, DataCryptor&& cryptor)
{
    workspace_cryptors_.insert_or_assign(workspace_id, std::move(cryptor));
}

//--------------------------------------------------------------------------------------------------
void RouterKeys::dropKeysExcept(const std::set<qint64>& visible_ids)
{
    std::erase_if(workspace_cryptors_, [&visible_ids](const auto& item)
    {
        return !visible_ids.contains(item.first);
    });
}

//--------------------------------------------------------------------------------------------------
void RouterKeys::resealGroupKeys(const QByteArray& new_public_key,
                                 proto::router::User* user) const
{
    CHECK(user);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, user);
}

//--------------------------------------------------------------------------------------------------
void RouterKeys::resealGroupKeys(const QByteArray& new_public_key,
                                 proto::router::ChangePasswordRequest* request) const
{
    CHECK(request);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, request);
}
