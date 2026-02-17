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

#include "base/crypto/message_encryptor.h"

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
MessageEncryptor::MessageEncryptor(
    MessageEncryptor::Type type, EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv)
    : type_(type),
      ctx_(std::move(ctx)),
      iv_(iv)
{
    DCHECK_EQ(EVP_CIPHER_CTX_key_length(ctx_.get()), kKeySize);
    DCHECK_EQ(EVP_CIPHER_CTX_iv_length(ctx_.get()), kIVSize);
}

//--------------------------------------------------------------------------------------------------
MessageEncryptor::~MessageEncryptor() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<MessageEncryptor> MessageEncryptor::createForAes256Gcm(
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

    return std::unique_ptr<MessageEncryptor>(new MessageEncryptor(
        MessageEncryptor::Type::AES256_GCM, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<MessageEncryptor> MessageEncryptor::createForChaCha20Poly1305(
    const QByteArray& key, const QByteArray& iv)
{
    if (key.size() != kKeySize || iv.size() != kIVSize)
    {
        LOG(ERROR) << "Key size:" << key.size() << "IV size:" << iv.size();
        return nullptr;
    }

    EVP_CIPHER_CTX_ptr ctx = createCipher(
        CipherType::CHACHA20_POLY1305, CipherMode::ENCRYPT, key, kIVSize);
    if (!ctx)
    {
        LOG(ERROR) << "createCipher failed";
        return nullptr;
    }

    return std::unique_ptr<MessageEncryptor>(new MessageEncryptor(
        MessageEncryptor::Type::CHACHA20_POLY1305, std::move(ctx), iv));
}

//--------------------------------------------------------------------------------------------------
size_t MessageEncryptor::encryptedDataSize(size_t in_size)
{
    return in_size + kTagSize;
}

//--------------------------------------------------------------------------------------------------
bool MessageEncryptor::encrypt(const void* in, size_t in_size, void* out)
{
    if (EVP_EncryptInit_ex(ctx_.get(), nullptr, nullptr, nullptr,
        reinterpret_cast<const quint8*>(iv_.data())) != 1)
    {
        LOG(ERROR) << "EVP_EncryptInit_ex failed";
        return false;
    }

    int length;

    if (EVP_EncryptUpdate(ctx_.get(),
                          reinterpret_cast<quint8*>(out) + kTagSize, &length,
                          reinterpret_cast<const quint8*>(in), static_cast<int>(in_size)) != 1)
    {
        LOG(ERROR) << "EVP_EncryptUpdate failed";
        return false;
    }

    if (EVP_EncryptFinal_ex(ctx_.get(),
                            reinterpret_cast<quint8*>(out) + kTagSize + length,
                            &length) != 1)
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
    return true;
}

} // namespace base
