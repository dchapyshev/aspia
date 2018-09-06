//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/cryptor_chacha20_poly1305.h"

#include <openssl/evp.h>

namespace aspia {

namespace {

const size_t kKeySize = 32; // 256 bits, 32 bytes.
const size_t kIVSize = 12; // 96 bits, 12 bytes.
const size_t kTagSize = 12; // 96 bits, 12 bytes.

EVP_CIPHER_CTX_ptr createCipher(const std::string& key, int type)
{
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx)
        return nullptr;

    if (EVP_CipherInit_ex(ctx.get(), EVP_chacha20_poly1305(),
                          nullptr, nullptr, nullptr, type) != 1)
    {
        qWarning("EVP_EncryptInit_ex failed");
        return nullptr;
    }

    if (EVP_CIPHER_CTX_set_key_length(ctx.get(), kKeySize) != 1)
    {
        qWarning("EVP_CIPHER_CTX_set_key_length failed");
        return nullptr;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, kIVSize, nullptr) != 1)
    {
        qWarning("EVP_CIPHER_CTX_ctrl failed");
        return nullptr;
    }

    if (EVP_CipherInit_ex(ctx.get(), nullptr, nullptr,
                          reinterpret_cast<const uint8_t*>(key.c_str()), nullptr, type) != 1)
    {
        qWarning("EVP_CIPHER_CTX_ctrl failed");
        return nullptr;
    }

    return ctx;
}

void incrementNonce(uint8_t* nonce)
{
    const union
    {
        long one;
        char little;
    } is_endian = { 1 };

    if (is_endian.little || (reinterpret_cast<size_t>(nonce) % sizeof(size_t)) != 0)
    {
        uint32_t n = kIVSize;
        uint32_t c = 1;

        do
        {
            --n;
            c += nonce[n];
            nonce[n] = static_cast<uint8_t>(c);
            c >>= 8;
        }
        while (n);
    }
    else
    {
        size_t* data = reinterpret_cast<size_t*>(nonce);
        size_t n = kIVSize / sizeof(size_t);
        size_t c = 1;

        do
        {
            --n;
            size_t d = data[n] += c;
            c = ((d - c) & ~d) >> (sizeof(size_t) * 8 - 1);
        }
        while (n);
    }
}

} // namespace

CryptorChaCha20Poly1305::CryptorChaCha20Poly1305(EVP_CIPHER_CTX_ptr encrypt_ctx,
                                                 EVP_CIPHER_CTX_ptr decrypt_ctx,
                                                 const std::string& encrypt_nonce,
                                                 const std::string& decrypt_nonce)
    : encrypt_ctx_(std::move(encrypt_ctx)),
      decrypt_ctx_(std::move(decrypt_ctx)),
      encrypt_nonce_(encrypt_nonce),
      decrypt_nonce_(decrypt_nonce)
{
    Q_ASSERT(EVP_CIPHER_CTX_key_length(encrypt_ctx_.get()) == kKeySize);
    Q_ASSERT(EVP_CIPHER_CTX_iv_length(encrypt_ctx_.get()) == kIVSize);
    Q_ASSERT(EVP_CIPHER_CTX_key_length(decrypt_ctx_.get()) == kKeySize);
    Q_ASSERT(EVP_CIPHER_CTX_iv_length(decrypt_ctx_.get()) == kIVSize);
}

CryptorChaCha20Poly1305::~CryptorChaCha20Poly1305() = default;

// static
Cryptor* CryptorChaCha20Poly1305::create(const std::string& key,
                                         const std::string& encrypt_iv,
                                         const std::string& decrypt_iv)
{
    if (key.size() != kKeySize || encrypt_iv.size() != kIVSize || decrypt_iv.size() != kIVSize)
        return nullptr;

    EVP_CIPHER_CTX_ptr encrypt_ctx = createCipher(key, 1);
    EVP_CIPHER_CTX_ptr decrypt_ctx = createCipher(key, 0);

    if (!encrypt_ctx || !decrypt_ctx)
        return nullptr;

    return new CryptorChaCha20Poly1305(
        std::move(encrypt_ctx), std::move(decrypt_ctx), encrypt_iv, decrypt_iv);
}

size_t CryptorChaCha20Poly1305::encryptedDataSize(size_t in_size)
{
    return in_size + kTagSize;
}

bool CryptorChaCha20Poly1305::encrypt(const char* in, size_t in_size, char* out)
{
    if (EVP_EncryptInit_ex(encrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const uint8_t*>(encrypt_nonce_.c_str())) != 1)
    {
        qWarning("EVP_EncryptInit_ex failed");
        return false;
    }

    int length;

    if (EVP_EncryptUpdate(encrypt_ctx_.get(),
                          reinterpret_cast<uint8_t*>(out),
                          &length,
                          reinterpret_cast<const uint8_t*>(in),
                          in_size) != 1)
    {
        qWarning("EVP_EncryptUpdate failed");
        return false;
    }

    if (EVP_EncryptFinal_ex(encrypt_ctx_.get(),
                            reinterpret_cast<uint8_t*>(out) + length,
                            &length) != 1)
    {
        qWarning("EVP_EncryptFinal_ex failed");
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(encrypt_ctx_.get(),
                            EVP_CTRL_AEAD_GET_TAG,
                            kTagSize,
                            reinterpret_cast<uint8_t*>(out) + in_size) != 1)
    {
        qWarning("EVP_CIPHER_CTX_ctrl failed");
        return false;
    }

    incrementNonce(reinterpret_cast<uint8_t*>(encrypt_nonce_.data()));
    return true;
}

size_t CryptorChaCha20Poly1305::decryptedDataSize(size_t in_size)
{
    return in_size - kTagSize;
}

bool CryptorChaCha20Poly1305::decrypt(const char* in, size_t in_size, char* out)
{
    if (EVP_DecryptInit_ex(decrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const uint8_t*>(decrypt_nonce_.c_str())) != 1)
    {
        qWarning("EVP_DecryptInit_ex failed");
        return false;
    }

    int length;

    if (EVP_DecryptUpdate(decrypt_ctx_.get(),
                          reinterpret_cast<uint8_t*>(out),
                          &length,
                          reinterpret_cast<const uint8_t*>(in),
                          in_size - kTagSize) != 1)
    {
        qWarning("EVP_DecryptUpdate failed");
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(decrypt_ctx_.get(),
                            EVP_CTRL_AEAD_SET_TAG,
                            kTagSize,
                            reinterpret_cast<uint8_t*>(const_cast<char*>(in)) +
                                in_size - kTagSize) != 1)
    {
        qWarning("EVP_CIPHER_CTX_ctrl failed");
        return false;
    }

    if (EVP_DecryptFinal_ex(decrypt_ctx_.get(),
                            reinterpret_cast<uint8_t*>(out) + length,
                            &length) <= 0)
    {
        qWarning("EVP_DecryptFinal_ex failed");
        return false;
    }

    incrementNonce(reinterpret_cast<uint8_t*>(decrypt_nonce_.data()));
    return true;
}

} // namespace aspia
