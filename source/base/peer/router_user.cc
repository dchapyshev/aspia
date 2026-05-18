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

#include "base/peer/router_user.h"

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "proto/router_admin.h"

//--------------------------------------------------------------------------------------------------
// static
RouterUser RouterUser::create(const QString& name, const SecureString& password)
{
    User base = User::create(name, password);
    if (!base.isValid())
        return RouterUser();

    KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        LOG(ERROR) << "Unable to generate key pair";
        return RouterUser();
    }

    RouterUser user;
    static_cast<User&>(user) = std::move(base);
    user.public_key = key_pair.publicKey();
    user.wrap_salt = PrivateKeyCryptor::generateSalt();
    user.wrap_private_key = PrivateKeyCryptor::encrypt(key_pair.privateKey(), password, user.wrap_salt);
    if (user.wrap_private_key.isEmpty())
    {
        LOG(ERROR) << "Unable to encrypt private key";
        return RouterUser();
    }

    return user;
}

//--------------------------------------------------------------------------------------------------
bool RouterUser::isValid() const
{
    return User::isValid() &&
           !public_key.isEmpty() &&
           !wrap_private_key.isEmpty() &&
           !wrap_salt.isEmpty();
}

//--------------------------------------------------------------------------------------------------
// static
RouterUser RouterUser::parseFrom(const proto::router::User& serialized_user)
{
    RouterUser user;

    user.entry_id         = serialized_user.entry_id();
    user.name             = QString::fromStdString(serialized_user.name());
    user.group            = QString::fromStdString(serialized_user.group());
    user.salt             = QByteArray::fromStdString(serialized_user.salt());
    user.verifier         = QByteArray::fromStdString(serialized_user.verifier());
    user.sessions         = serialized_user.sessions();
    user.flags            = serialized_user.flags();
    user.public_key       = QByteArray::fromStdString(serialized_user.public_key());
    user.wrap_private_key = QByteArray::fromStdString(serialized_user.wrap_private_key());
    user.wrap_salt        = QByteArray::fromStdString(serialized_user.wrap_salt());

    return user;
}

//--------------------------------------------------------------------------------------------------
proto::router::User RouterUser::serialize() const
{
    proto::router::User user;

    user.set_entry_id(entry_id);
    user.set_name(name.toStdString());
    user.set_group(group.toStdString());
    user.set_salt(salt.toStdString());
    user.set_verifier(verifier.toStdString());
    user.set_sessions(sessions);
    user.set_flags(flags);
    user.set_public_key(public_key.toStdString());
    user.set_wrap_private_key(wrap_private_key.toStdString());
    user.set_wrap_salt(wrap_salt.toStdString());

    return user;
}
