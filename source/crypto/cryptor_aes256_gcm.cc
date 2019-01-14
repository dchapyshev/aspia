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

#include "crypto/cryptor_aes256_gcm.h"

#include <openssl/evp.h>

#include "base/logging.h"

namespace crypto {

namespace {

const int kKeySize = 32; // 256 bits, 32 bytes.
const int kIVSize = 12; // 96 bits, 12 bytes.
const int kTagSize = 16; // 128 bits, 16 bytes.

EVP_CIPHER_CTX_ptr createCipher(const QByteArray& key, int type)
{
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_new failed";
        return nullptr;
    }

    if (EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(),
                          nullptr, nullptr, nullptr, type) != 1)
    {
        LOG(LS_WARNING) << "EVP_EncryptInit_ex failed";
        return nullptr;
    }

    if (EVP_CIPHER_CTX_set_key_length(ctx.get(), kKeySize) != 1)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_set_key_length failed";
        return nullptr;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, kIVSize, nullptr) != 1)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_ctrl failed";
        return nullptr;
    }

    if (EVP_CipherInit_ex(ctx.get(), nullptr, nullptr,
                          reinterpret_cast<const uint8_t*>(key.constData()), nullptr, type) != 1)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_ctrl failed";
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

CryptorAes256Gcm::CryptorAes256Gcm(EVP_CIPHER_CTX_ptr encrypt_ctx,
                                   EVP_CIPHER_CTX_ptr decrypt_ctx,
                                   const QByteArray& encrypt_nonce,
                                   const QByteArray& decrypt_nonce)
    : encrypt_ctx_(std::move(encrypt_ctx)),
      decrypt_ctx_(std::move(decrypt_ctx)),
      encrypt_nonce_(encrypt_nonce),
      decrypt_nonce_(decrypt_nonce)
{
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(encrypt_ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(encrypt_ctx_.get()), kIVSize);
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(decrypt_ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(decrypt_ctx_.get()), kIVSize);
}

CryptorAes256Gcm::~CryptorAes256Gcm() = default;

// static
Cryptor* CryptorAes256Gcm::create(const QByteArray& key,
                                  const QByteArray& encrypt_iv,
                                  const QByteArray& decrypt_iv)
{
    if (key.size() != kKeySize || encrypt_iv.size() != kIVSize || decrypt_iv.size() != kIVSize)
    {
        LOG(LS_WARNING) << "Invalid parameters. Key: " << key.size()
                        << " Encrypt IV: " << encrypt_iv.size()
                        << " Decrypt IV: " << decrypt_iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr encrypt_ctx = createCipher(key, 1);
    EVP_CIPHER_CTX_ptr decrypt_ctx = createCipher(key, 0);

    if (!encrypt_ctx || !decrypt_ctx)
        return nullptr;

    return new CryptorAes256Gcm(
        std::move(encrypt_ctx), std::move(decrypt_ctx), encrypt_iv, decrypt_iv);
}

size_t CryptorAes256Gcm::encryptedDataSize(size_t in_size)
{
    return in_size + kTagSize;
}

bool CryptorAes256Gcm::encrypt(const char* in, size_t in_size, char* out)
{
    if (EVP_EncryptInit_ex(encrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const uint8_t*>(encrypt_nonce_.constData())) != 1)
    {
        LOG(LS_WARNING) << "EVP_EncryptInit_ex failed";
        return false;
    }

    int length;

    if (EVP_EncryptUpdate(encrypt_ctx_.get(),
                          reinterpret_cast<uint8_t*>(out) + kTagSize,
                          &length,
                          reinterpret_cast<const uint8_t*>(in),
                          in_size) != 1)
    {
        LOG(LS_WARNING) << "EVP_EncryptUpdate failed";
        return false;
    }

    if (EVP_EncryptFinal_ex(encrypt_ctx_.get(),
                            reinterpret_cast<uint8_t*>(out) + kTagSize + length,
                            &length) != 1)
    {
        LOG(LS_WARNING) << "EVP_EncryptFinal_ex failed";
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(encrypt_ctx_.get(),
                            EVP_CTRL_AEAD_GET_TAG,
                            kTagSize,
                            reinterpret_cast<uint8_t*>(out)) != 1)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_ctrl failed";
        return false;
    }

    incrementNonce(reinterpret_cast<uint8_t*>(encrypt_nonce_.data()));
    return true;
}

size_t CryptorAes256Gcm::decryptedDataSize(size_t in_size)
{
    return in_size - kTagSize;
}

bool CryptorAes256Gcm::decrypt(const char* in, size_t in_size, char* out)
{
    if (EVP_DecryptInit_ex(decrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const uint8_t*>(decrypt_nonce_.constData())) != 1)
    {
        LOG(LS_WARNING) << "EVP_DecryptInit_ex failed";
        return false;
    }

    int length;

    if (EVP_DecryptUpdate(decrypt_ctx_.get(),
                          reinterpret_cast<uint8_t*>(out),
                          &length,
                          reinterpret_cast<const uint8_t*>(in) + kTagSize,
                          in_size - kTagSize) != 1)
    {
        LOG(LS_WARNING) << "EVP_DecryptUpdate failed";
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(decrypt_ctx_.get(),
                            EVP_CTRL_AEAD_SET_TAG,
                            kTagSize,
                            reinterpret_cast<uint8_t*>(const_cast<char*>(in))) != 1)
    {
        LOG(LS_WARNING) << "EVP_CIPHER_CTX_ctrl failed";
        return false;
    }

    if (EVP_DecryptFinal_ex(decrypt_ctx_.get(),
                            reinterpret_cast<uint8_t*>(out) + length,
                            &length) <= 0)
    {
        LOG(LS_WARNING) << "EVP_DecryptFinal_ex failed";
        return false;
    }

    incrementNonce(reinterpret_cast<uint8_t*>(decrypt_nonce_.data()));
    return true;
}

} // namespace crypto
