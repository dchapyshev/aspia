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

#ifndef BASE_CRYPTO_MESSAGE_DECRYPTOR_H
#define BASE_CRYPTO_MESSAGE_DECRYPTOR_H

#include <QByteArray>
#include <QObject>

#include "base/crypto/openssl_util.h"

namespace base {

class MessageDecryptor
{
    Q_GADGET

public:
    ~MessageDecryptor();

    enum class Type
    {
        UNKNOWN,
        FAKE,
        AES256_GCM,
        CHACHA20_POLY1305
    };
    Q_ENUM(Type)

    static std::unique_ptr<MessageDecryptor> createForAes256Gcm(
        const QByteArray& key, const QByteArray& iv);

    static std::unique_ptr<MessageDecryptor> createForChaCha20Poly1305(
        const QByteArray& key, const QByteArray& iv);

    Type type() const { return type_; }

    size_t decryptedDataSize(size_t in_size);
    bool decrypt(const void* in, size_t in_size, void* out);

private:
    MessageDecryptor(MessageDecryptor::Type type, EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv);

    const Type type_;
    EVP_CIPHER_CTX_ptr ctx_;
    QByteArray iv_;

    Q_DISABLE_COPY(MessageDecryptor)
};

} // namespace base

#endif // BASE_CRYPTO_MESSAGE_DECRYPTOR_H
