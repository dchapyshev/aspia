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

#include "base/crypto/private_key_cryptor.h"

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"

//--------------------------------------------------------------------------------------------------
// static
QByteArray PrivateKeyCryptor::generateSalt()
{
    return Random::byteArray(kSaltSize);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray PrivateKeyCryptor::encrypt(const SecureByteArray& private_key,
                                      const SecureString& password,
                                      const QByteArray& salt)
{
    if (private_key.isEmpty())
    {
        LOG(ERROR) << "Empty private key";
        return QByteArray();
    }

    if (password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return QByteArray();
    }

    if (salt.size() != kSaltSize)
    {
        LOG(ERROR) << "Invalid salt size:" << salt.size();
        return QByteArray();
    }

    SecureByteArray derived_key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    if (derived_key.isEmpty())
    {
        LOG(ERROR) << "Argon2id derivation failed";
        return QByteArray();
    }

    DataCryptor cryptor(derived_key);
    std::optional<QByteArray> encrypted = cryptor.encrypt(private_key.toByteArray());
    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Unable to encrypt private key";
        return QByteArray();
    }

    return *encrypted;
}

//--------------------------------------------------------------------------------------------------
// static
SecureByteArray PrivateKeyCryptor::decrypt(const QByteArray& encrypted_private_key,
                                           const SecureString& password,
                                           const QByteArray& salt)
{
    if (encrypted_private_key.isEmpty())
    {
        LOG(ERROR) << "Empty encrypted private key";
        return SecureByteArray();
    }

    if (password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return SecureByteArray();
    }

    if (salt.size() != kSaltSize)
    {
        LOG(ERROR) << "Invalid salt size:" << salt.size();
        return SecureByteArray();
    }

    SecureByteArray derived_key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    if (derived_key.isEmpty())
    {
        LOG(ERROR) << "Argon2id derivation failed";
        return SecureByteArray();
    }

    DataCryptor cryptor(derived_key);
    std::optional<QByteArray> decrypted = cryptor.decrypt(encrypted_private_key);
    if (!decrypted.has_value())
    {
        LOG(ERROR) << "Unable to decrypt private key";
        return SecureByteArray();
    }

    return SecureByteArray(std::move(*decrypted));
}
