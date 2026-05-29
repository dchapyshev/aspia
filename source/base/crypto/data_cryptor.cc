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

#include "base/crypto/data_cryptor.h"

#include "base/logging.h"
#include "base/crypto/random.h"

#include <openssl/evp.h>

namespace {

const int kKeySize = 32; // 256 bits, 32 bytes.
const int kIVSize = 12; // 96 bits, 12 bytes.
const int kTagSize = 16; // 128 bits, 16 bytes.
const int kHeaderSize = kIVSize + kTagSize;

} // namespace

//--------------------------------------------------------------------------------------------------
DataCryptor::DataCryptor(CipherType type)
    : type_(type)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DataCryptor::DataCryptor(CipherType type, const SecureByteArray& key)
    : type_(type)
{
    setKey(key);
}

//--------------------------------------------------------------------------------------------------
DataCryptor::DataCryptor(DataCryptor&& other) noexcept
{
    std::scoped_lock lock(other.mutex_);
    type_        = other.type_;
    key_         = std::move(other.key_);
    encrypt_ctx_ = std::move(other.encrypt_ctx_);
    decrypt_ctx_ = std::move(other.decrypt_ctx_);
}

//--------------------------------------------------------------------------------------------------
DataCryptor& DataCryptor::operator=(DataCryptor&& other) noexcept
{
    if (this == &other)
        return *this;

    std::scoped_lock lock(mutex_, other.mutex_);
    type_        = other.type_;
    key_         = std::move(other.key_);
    encrypt_ctx_ = std::move(other.encrypt_ctx_);
    decrypt_ctx_ = std::move(other.decrypt_ctx_);
    return *this;
}

//--------------------------------------------------------------------------------------------------
DataCryptor::~DataCryptor() = default;

//--------------------------------------------------------------------------------------------------
void DataCryptor::setKey(const SecureByteArray& key)
{
    std::scoped_lock lock(mutex_);

    key_ = key;

    encrypt_ctx_.reset();
    decrypt_ctx_.reset();

    if (key.isEmpty())
        return;

    if (key.size() != kKeySize)
    {
        LOG(ERROR) << "Wrong key size:" << key.size();
        return;
    }

    encrypt_ctx_ = createCipher(type_, CipherMode::ENCRYPT, key, kIVSize);
    if (!encrypt_ctx_)
        LOG(ERROR) << "Unable to create encrypt cipher";

    decrypt_ctx_ = createCipher(type_, CipherMode::DECRYPT, key, kIVSize);
    if (!decrypt_ctx_)
        LOG(ERROR) << "Unable to create decrypt cipher";
}

//--------------------------------------------------------------------------------------------------
SecureByteArray DataCryptor::key() const
{
    std::scoped_lock lock(mutex_);
    return key_;
}

//--------------------------------------------------------------------------------------------------
bool DataCryptor::isValid() const
{
    std::scoped_lock lock(mutex_);
    return encrypt_ctx_ && decrypt_ctx_;
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> DataCryptor::encrypt(QByteArrayView in) const
{
    std::scoped_lock lock(mutex_);

    if (in.empty())
    {
        LOG(ERROR) << "Empty buffer passed";
        return std::nullopt;
    }

    if (!encrypt_ctx_)
    {
        LOG(ERROR) << "Encrypt cipher is not initialized";
        return std::nullopt;
    }

    QByteArray out;
    out.resize(in.size() + kHeaderSize);

    if (!Random::fillBuffer(out.data(), kIVSize))
    {
        LOG(ERROR) << "Random::fillBuffer failed";
        return std::nullopt;
    }

    if (EVP_EncryptInit_ex(encrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const quint8*>(out.data())) != 1)
    {
        LOG(ERROR) << "EVP_EncryptInit_ex failed";
        return std::nullopt;
    }

    int length;

    if (EVP_EncryptUpdate(encrypt_ctx_.get(),
                          reinterpret_cast<quint8*>(out.data()) + kHeaderSize,
                          &length,
                          reinterpret_cast<const quint8*>(in.data()),
                          static_cast<int>(in.size())) != 1)
    {
        LOG(ERROR) << "EVP_EncryptUpdate failed";
        return std::nullopt;
    }

    if (EVP_EncryptFinal_ex(encrypt_ctx_.get(),
                            reinterpret_cast<quint8*>(out.data()) + kHeaderSize + length,
                            &length) != 1)
    {
        LOG(ERROR) << "EVP_EncryptFinal_ex failed";
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(encrypt_ctx_.get(),
                            EVP_CTRL_AEAD_GET_TAG,
                            kTagSize,
                            reinterpret_cast<quint8*>(out.data()) + kIVSize) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return std::nullopt;
    }

    return out;
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> DataCryptor::decrypt(QByteArrayView in) const
{
    std::scoped_lock lock(mutex_);

    if (in.size() <= kHeaderSize)
    {
        LOG(ERROR) << "Header missed";
        return std::nullopt;
    }

    if (!decrypt_ctx_)
    {
        LOG(ERROR) << "Decrypt cipher is not initialized";
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(decrypt_ctx_.get(), nullptr, nullptr, nullptr,
                           reinterpret_cast<const quint8*>(in.data())) != 1)
    {
        LOG(ERROR) << "EVP_DecryptInit_ex failed";
        return std::nullopt;
    }

    QByteArray out;
    out.resize(in.size() - kHeaderSize);

    int length;

    if (EVP_DecryptUpdate(decrypt_ctx_.get(),
                          reinterpret_cast<quint8*>(out.data()),
                          &length,
                          reinterpret_cast<const quint8*>(in.data()) + kHeaderSize,
                          static_cast<int>(in.size() - kHeaderSize)) != 1)
    {
        LOG(ERROR) << "EVP_DecryptUpdate failed";
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(decrypt_ctx_.get(),
                            EVP_CTRL_AEAD_SET_TAG,
                            kTagSize,
                            reinterpret_cast<quint8*>(
                                const_cast<char*>(in.data())) + kIVSize) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return std::nullopt;
    }

    if (EVP_DecryptFinal_ex(decrypt_ctx_.get(),
                            reinterpret_cast<quint8*>(out.data()) + length,
                            &length) <= 0)
    {
        LOG(ERROR) << "EVP_DecryptFinal_ex failed";
        return std::nullopt;
    }

    return out;
}

//--------------------------------------------------------------------------------------------------
// static
DataCryptor& DataCryptor::instance()
{
    // Process-wide singleton so the master key is shared across all threads (GUI and IO).
    // EVP_CIPHER_CTX state is guarded by the per-instance mutex. AES256 GCM is pinned
    // here because the existing client database was written with that cipher.
    static DataCryptor cryptor(CipherType::AES256_GCM);
    return cryptor;
}
