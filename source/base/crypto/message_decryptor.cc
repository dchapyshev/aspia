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

#include "base/crypto/message_decryptor.h"

#include "base/logging.h"
#include "base/crypto/large_number_increment.h"

#include <openssl/evp.h>

namespace base {

namespace {

const int kKeySize = 32; // 256 bits, 32 bytes.
const int kIVSize = 12; // 96 bits, 12 bytes.
const int kTagSize = 16; // 128 bits, 16 bytes.

} // namespace

//--------------------------------------------------------------------------------------------------
MessageDecryptor::MessageDecryptor(
    MessageDecryptor::Type type, EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv)
    : type_(type),
      ctx_(std::move(ctx)),
      iv_(iv)
{
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(ctx_.get()), kIVSize);
}

//--------------------------------------------------------------------------------------------------
MessageDecryptor::~MessageDecryptor() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<MessageDecryptor> MessageDecryptor::createForAes256Gcm(
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

    return std::unique_ptr<MessageDecryptor>(
        new MessageDecryptor(MessageDecryptor::Type::AES256_GCM, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<MessageDecryptor> MessageDecryptor::createForChaCha20Poly1305(
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

    return std::unique_ptr<MessageDecryptor>(
        new MessageDecryptor(MessageDecryptor::Type::CHACHA20_POLY1305, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
size_t MessageDecryptor::decryptedDataSize(size_t in_size)
{
    return in_size - kTagSize;
}

//--------------------------------------------------------------------------------------------------
bool MessageDecryptor::decrypt(const void* in, size_t in_size, void* out)
{
    if (EVP_DecryptInit_ex(ctx_.get(), nullptr, nullptr, nullptr,
        reinterpret_cast<const quint8*>(iv_.data())) != 1)
    {
        LOG(ERROR) << "EVP_DecryptInit_ex failed";
        return false;
    }

    int length;

    if (EVP_DecryptUpdate(ctx_.get(),
                          reinterpret_cast<quint8*>(out), &length,
                          reinterpret_cast<const quint8*>(in) + kTagSize,
                          static_cast<int>(in_size) - kTagSize) != 1)
    {
        LOG(ERROR) << "EVP_DecryptUpdate failed";
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_AEAD_SET_TAG, kTagSize,
                            const_cast<void*>(in)) != 1)
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
