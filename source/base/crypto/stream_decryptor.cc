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

#include "base/crypto/stream_decryptor.h"

#include "base/logging.h"
#include "base/crypto/large_number_increment.h"

#include <openssl/evp.h>

namespace base {

namespace {

const qint64 kKeySize = 32; // 256 bits, 32 bytes.
const qint64 kIVSize = 12; // 96 bits, 12 bytes.
const qint64 kTagSize = 16; // 128 bits, 16 bytes.

} // namespace

//--------------------------------------------------------------------------------------------------
StreamDecryptor::StreamDecryptor(
    StreamDecryptor::Type type, EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv)
    : type_(type),
      ctx_(std::move(ctx)),
      iv_(iv)
{
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(ctx_.get()), kIVSize);
}

//--------------------------------------------------------------------------------------------------
StreamDecryptor::~StreamDecryptor() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<StreamDecryptor> StreamDecryptor::createForAes256Gcm(
    const QByteArray& key, const QByteArray& iv)
{
    if (key.size() != kKeySize || iv.size() != kIVSize)
    {
        LOG(ERROR) << "Key size:" << key.size() << "IV size:" << iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr ctx =
        createCipher(CipherType::AES256_GCM, CipherMode::DECRYPT, key, kIVSize);
    if (!ctx)
    {
        LOG(ERROR) << "createCipher failed";
        return nullptr;
    }

    return std::unique_ptr<StreamDecryptor>(
        new StreamDecryptor(StreamDecryptor::Type::AES256_GCM, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<StreamDecryptor> StreamDecryptor::createForChaCha20Poly1305(
    const QByteArray& key, const QByteArray& iv)
{
    if (key.size() != kKeySize || iv.size() != kIVSize)
    {
        LOG(ERROR) << "Key size:" << key.size() << "IV size:" << iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr ctx =
        createCipher(CipherType::CHACHA20_POLY1305, CipherMode::DECRYPT, key, kIVSize);
    if (!ctx)
    {
        LOG(ERROR) << "createCipher failed";
        return nullptr;
    }

    return std::unique_ptr<StreamDecryptor>(
        new StreamDecryptor(StreamDecryptor::Type::CHACHA20_POLY1305, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
qint64 StreamDecryptor::decryptedDataSize(qint64 in_size)
{
    return in_size - kTagSize;
}

//--------------------------------------------------------------------------------------------------
bool StreamDecryptor::decrypt(const void* in, qint64 in_size, void* out)
{
    return decrypt(in, in_size, nullptr, 0, out);
}

//--------------------------------------------------------------------------------------------------
bool StreamDecryptor::decrypt(const void* in, qint64 in_size, const void* aad, qint64 aad_size, void* out)
{
    if (EVP_DecryptInit_ex(ctx_.get(), nullptr, nullptr, nullptr,
        reinterpret_cast<const quint8*>(iv_.data())) != 1)
    {
        LOG(ERROR) << "EVP_DecryptInit_ex failed";
        return false;
    }

    int length;

    if (aad && aad_size > 0)
    {
        if (EVP_DecryptUpdate(ctx_.get(), nullptr, &length, reinterpret_cast<const quint8*>(aad),
            static_cast<int>(aad_size)) != 1)
        {
            LOG(ERROR) << "EVP_DecryptUpdate (AAD) failed";
            return false;
        }
    }

    if (EVP_DecryptUpdate(ctx_.get(), reinterpret_cast<quint8*>(out), &length,
        reinterpret_cast<const quint8*>(in) + kTagSize, static_cast<int>(in_size - kTagSize)) != 1)
    {
        LOG(ERROR) << "EVP_DecryptUpdate failed";
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_AEAD_SET_TAG, kTagSize, const_cast<void*>(in)) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return false;
    }

    if (EVP_DecryptFinal_ex(ctx_.get(), reinterpret_cast<quint8*>(out) + length, &length) <= 0)
    {
        LOG(ERROR) << "EVP_DecryptFinal_ex failed";
        return false;
    }

    largeNumberIncrement(&iv_);
    return true;
}

} // namespace base
