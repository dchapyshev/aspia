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

#ifndef BASE_CRYPTO_KEY_PAIR_H
#define BASE_CRYPTO_KEY_PAIR_H

#include "base/crypto/openssl_util.h"

namespace base {

class KeyPair
{
public:
    KeyPair();
    KeyPair(KeyPair&& other) noexcept;
    KeyPair& operator=(KeyPair&& other) noexcept;
    ~KeyPair();

    enum class Type { X25519 };

    static KeyPair create(Type type);
    static KeyPair fromPrivateKey(const QByteArray& private_key);

    bool isValid() const;
    QByteArray privateKey() const;
    QByteArray publicKey() const;
    QByteArray sessionKey(const QByteArray& peer_public_key) const;

private:
    explicit KeyPair(EVP_PKEY_ptr&& pkey) noexcept;

    EVP_PKEY_ptr pkey_;

    Q_DISABLE_COPY(KeyPair)
};

} // namespace base

#endif // BASE_CRYPTO_KEY_PAIR_H
