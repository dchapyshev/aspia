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

#include "base/crypto/stream_encryptor.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/secure_memory.h"

#include <openssl/evp.h>

namespace {

const qint64 kKeySize = 32; // 256 bits, 32 bytes.
const qint64 kIVSize = 12; // 96 bits, 12 bytes.
const qint64 kTagSize = 16; // 128 bits, 16 bytes.

// Number of frames before the working key is ratcheted forward for forward secrecy. Low enough
// that idle connections (only keep-alive pings flowing) still rotate keys within reasonable
// time; ratchet cost is a single BLAKE2s + AEAD context rebuild (~50 us), so frequent rotation
// on active streams is negligible CPU.
constexpr quint64 kRatchetInterval = 256;

} // namespace

//--------------------------------------------------------------------------------------------------
StreamEncryptor::StreamEncryptor(CipherType type, EVP_CIPHER_CTX_ptr ctx,
    const QByteArray& key, const QByteArray& iv)
    : type_(type),
      ctx_(std::move(ctx)),
      key_(key),
      iv_(iv)
{
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(ctx_.get()), kIVSize);
}

//--------------------------------------------------------------------------------------------------
StreamEncryptor::~StreamEncryptor()
{
    memZero(&key_);
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<StreamEncryptor> StreamEncryptor::createForAes256Gcm(
    const QByteArray& key, const QByteArray& iv)
{
    if (key.size() != kKeySize || iv.size() != kIVSize)
    {
        LOG(ERROR) << "Key size:" << key.size() << "IV size:" << iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr ctx = createCipher(CipherType::AES256_GCM, CipherMode::ENCRYPT, key, kIVSize);
    if (!ctx)
    {
        LOG(ERROR) << "createCipher failed";
        return nullptr;
    }

    return std::unique_ptr<StreamEncryptor>(new StreamEncryptor(
        CipherType::AES256_GCM, std::move(ctx), key, iv));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<StreamEncryptor> StreamEncryptor::createForChaCha20Poly1305(
    const QByteArray& key, const QByteArray& iv)
{
    if (key.size() != kKeySize || iv.size() != kIVSize)
    {
        LOG(ERROR) << "Key size:" << key.size() << "IV size:" << iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr ctx = createCipher(CipherType::CHACHA20_POLY1305, CipherMode::ENCRYPT, key, kIVSize);
    if (!ctx)
    {
        LOG(ERROR) << "createCipher failed";
        return nullptr;
    }

    return std::unique_ptr<StreamEncryptor>(new StreamEncryptor(
        CipherType::CHACHA20_POLY1305, std::move(ctx), key, iv));
}

//--------------------------------------------------------------------------------------------------
qint64 StreamEncryptor::encryptedDataSize(qint64 in_size)
{
    return in_size + kTagSize;
}

//--------------------------------------------------------------------------------------------------
bool StreamEncryptor::encrypt(const void* in, qint64 in_size, void* out)
{
    return encrypt(in, in_size, nullptr, 0, out);
}

//--------------------------------------------------------------------------------------------------
bool StreamEncryptor::encrypt(const void* in, qint64 in_size, const void* aad, qint64 aad_size, void* out)
{
    if (EVP_EncryptInit_ex(ctx_.get(), nullptr, nullptr, nullptr,
        reinterpret_cast<const quint8*>(iv_.data())) != 1)
    {
        LOG(ERROR) << "EVP_EncryptInit_ex failed";
        return false;
    }

    int length;

    if (aad && aad_size > 0)
    {
        if (EVP_EncryptUpdate(ctx_.get(), nullptr, &length, reinterpret_cast<const quint8*>(aad),
            static_cast<int>(aad_size)) != 1)
        {
            LOG(ERROR) << "EVP_EncryptUpdate (AAD) failed";
            return false;
        }
    }

    if (EVP_EncryptUpdate(ctx_.get(), reinterpret_cast<quint8*>(out) + kTagSize, &length,
        reinterpret_cast<const quint8*>(in), static_cast<int>(in_size)) != 1)
    {
        LOG(ERROR) << "EVP_EncryptUpdate failed";
        return false;
    }

    if (EVP_EncryptFinal_ex(ctx_.get(), reinterpret_cast<quint8*>(out) + kTagSize + length, &length) != 1)
    {
        LOG(ERROR) << "EVP_EncryptFinal_ex failed";
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_AEAD_GET_TAG, kTagSize, out) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return false;
    }

    largeNumberIncrement(&iv_);

    if (++msg_count_ >= kRatchetInterval)
    {
        GenericHash hash(GenericHash::BLAKE2s256);
        hash.addData(key_);
        hash.addData(QByteArrayLiteral("aspia/v1/ratchet"));
        QByteArray new_key = hash.result();

        EVP_CIPHER_CTX_ptr new_ctx = createCipher(type_, CipherMode::ENCRYPT, new_key, kIVSize);
        if (!new_ctx)
        {
            LOG(ERROR) << "ratchet: createCipher failed";
            memZero(&new_key);
            return false;
        }

        memZero(&key_);
        key_ = std::move(new_key);
        ctx_ = std::move(new_ctx);
        msg_count_ = 0;
    }

    return true;
}
