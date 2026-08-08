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
#include "base/crypto/private_key_cryptor.h"
#include "proto/router_client.h"

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

    return Result::OK;
}

//--------------------------------------------------------------------------------------------------
void RouterKeys::clear()
{
    user_id_ = 0;
    user_name_.clear();
    user_private_key_.clear();
}
