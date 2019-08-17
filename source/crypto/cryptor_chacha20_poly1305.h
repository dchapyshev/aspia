//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CRYPTO__CRYPTOR_CHACHA20_POLY1305_H
#define CRYPTO__CRYPTOR_CHACHA20_POLY1305_H

#include "base/byte_array.h"
#include "base/macros_magic.h"
#include "crypto/cryptor.h"
#include "crypto/openssl_util.h"

namespace crypto {

class CryptorChaCha20Poly1305 : public Cryptor
{
public:
    ~CryptorChaCha20Poly1305();

    static std::unique_ptr<Cryptor> create(
        base::ByteArray&& key, base::ByteArray&& encrypt_iv, base::ByteArray&& decrypt_iv);

    size_t encryptedDataSize(size_t in_size) override;
    bool encrypt(const uint8_t* in, size_t in_size, uint8_t* out) override;

    size_t decryptedDataSize(size_t in_size) override;
    bool decrypt(const uint8_t* in, size_t in_size, uint8_t* out) override;

protected:
    CryptorChaCha20Poly1305(EVP_CIPHER_CTX_ptr encrypt_ctx,
                            EVP_CIPHER_CTX_ptr decrypt_ctx,
                            base::ByteArray&& encrypt_nonce,
                            base::ByteArray&& decrypt_nonce);

private:
    EVP_CIPHER_CTX_ptr encrypt_ctx_;
    EVP_CIPHER_CTX_ptr decrypt_ctx_;

    base::ByteArray encrypt_nonce_;
    base::ByteArray decrypt_nonce_;

    DISALLOW_COPY_AND_ASSIGN(CryptorChaCha20Poly1305);
};

} // namespace crypto

#endif // CRYPTO__CRYPTOR_CHACHA20_POLY1305_H
