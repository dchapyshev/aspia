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

#ifndef BASE_CRYPTO_PRIVATE_KEY_CRYPTOR_H
#define BASE_CRYPTO_PRIVATE_KEY_CRYPTOR_H

#include <QByteArray>

class SecureByteArray;
class SecureString;

// Symmetric AEAD wrapper for a private key protected by a user password.
//
// The encryption key is derived from the password and a per-user salt via Argon2id, then used
// to encrypt the private key bytes with AES256 GCM. The salt must be persisted alongside
// the ciphertext so the key can be re-derived on decrypt.
class PrivateKeyCryptor
{
public:
    static constexpr int kSaltSize = 16;

    static QByteArray generateSalt();

    static QByteArray encrypt(const SecureByteArray& private_key,
                              const SecureString& password,
                              const QByteArray& salt);

    static SecureByteArray decrypt(const QByteArray& encrypted_private_key,
                                   const SecureString& password,
                                   const QByteArray& salt);

private:
    Q_DISABLE_COPY_MOVE(PrivateKeyCryptor)
};

#endif // BASE_CRYPTO_PRIVATE_KEY_CRYPTOR_H
