//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CRYPTO_MESSAGE_ENCRYPTOR_OPENSSL_H
#define BASE_CRYPTO_MESSAGE_ENCRYPTOR_OPENSSL_H

#include <QByteArray>

#include "base/macros_magic.h"
#include "base/crypto/message_encryptor.h"
#include "base/crypto/openssl_util.h"

namespace base {

class MessageEncryptorOpenssl final : public MessageEncryptor
{
public:
    ~MessageEncryptorOpenssl() final;

    static std::unique_ptr<MessageEncryptor> createForAes256Gcm(
        const QByteArray& key, const QByteArray& iv);

    static std::unique_ptr<MessageEncryptor> createForChaCha20Poly1305(
        const QByteArray& key, const QByteArray& iv);

    // MessageEncryptor implementation.
    size_t encryptedDataSize(size_t in_size) final;
    bool encrypt(const void* in, size_t in_size, void* out) final;

private:
    MessageEncryptorOpenssl(EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv);

    EVP_CIPHER_CTX_ptr ctx_;
    QByteArray iv_;

    DISALLOW_COPY_AND_ASSIGN(MessageEncryptorOpenssl);
};

} // namespace base

#endif // BASE_CRYPTO_MESSAGE_ENCRYPTOR_OPENSSL_H
