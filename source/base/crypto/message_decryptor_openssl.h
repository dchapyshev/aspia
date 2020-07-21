//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CRYPTO__MESSAGE_DECRYPTOR_OPENSSL_H
#define BASE__CRYPTO__MESSAGE_DECRYPTOR_OPENSSL_H

#include "base/macros_magic.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/openssl_util.h"
#include "base/memory/byte_array.h"

namespace base {

class MessageDecryptorOpenssl : public MessageDecryptor
{
public:
    ~MessageDecryptorOpenssl();

    static std::unique_ptr<MessageDecryptor> createForAes256Gcm(
        const ByteArray& key, const ByteArray& iv);

    static std::unique_ptr<MessageDecryptor> createForChaCha20Poly1305(
        const ByteArray& key, const ByteArray& iv);

    // MessageDecryptor implementation.
    size_t decryptedDataSize(size_t in_size) override;
    bool decrypt(const void* in, size_t in_size, void* out) override;

private:
    MessageDecryptorOpenssl(EVP_CIPHER_CTX_ptr ctx, const ByteArray& iv);

    EVP_CIPHER_CTX_ptr ctx_;
    ByteArray iv_;

    DISALLOW_COPY_AND_ASSIGN(MessageDecryptorOpenssl);
};

} // namespace base

#endif // BASE__CRYPTO__MESSAGE_DECRYPTOR_OPENSSL_H
