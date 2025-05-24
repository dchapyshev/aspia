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

#ifndef BASE_CRYPTO_MESSAGE_DECRYPTOR_OPENSSL_H
#define BASE_CRYPTO_MESSAGE_DECRYPTOR_OPENSSL_H

#include <QByteArray>

#include "base/macros_magic.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/openssl_util.h"

namespace base {

class MessageDecryptorOpenssl final : public MessageDecryptor
{
public:
    ~MessageDecryptorOpenssl() final;

    static std::unique_ptr<MessageDecryptor> createForAes256Gcm(
        const QByteArray& key, const QByteArray& iv);

    static std::unique_ptr<MessageDecryptor> createForChaCha20Poly1305(
        const QByteArray& key, const QByteArray& iv);

    // MessageDecryptor implementation.
    size_t decryptedDataSize(size_t in_size) final;
    bool decrypt(const void* in, size_t in_size, void* out) final;

private:
    MessageDecryptorOpenssl(EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv);

    EVP_CIPHER_CTX_ptr ctx_;
    QByteArray iv_;

    DISALLOW_COPY_AND_ASSIGN(MessageDecryptorOpenssl);
};

} // namespace base

#endif // BASE_CRYPTO_MESSAGE_DECRYPTOR_OPENSSL_H
