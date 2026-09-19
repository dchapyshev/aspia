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

#include "base/crypto/signature.h"

#include "base/logging.h"
#include "base/crypto/openssl_util.h"
#include "base/crypto/secure_byte_array.h"

#include <openssl/evp.h>

namespace {

const qsizetype kKeySize = 32;
const qsizetype kSignatureSize = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QByteArray Signature::publicKey(const SecureByteArray& private_key)
{
    if (private_key.size() != kKeySize)
    {
        LOG(ERROR) << "Invalid private key size:" << private_key.size();
        return QByteArray();
    }

    EVP_PKEY_ptr pkey(EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const quint8*>(private_key.constData()),
        static_cast<size_t>(private_key.size())));
    if (!pkey)
    {
        LOG(ERROR) << "EVP_PKEY_new_raw_private_key failed";
        return QByteArray();
    }

    QByteArray public_key;
    public_key.resize(kKeySize);

    size_t public_key_size = static_cast<size_t>(public_key.size());

    if (EVP_PKEY_get_raw_public_key(pkey.get(), reinterpret_cast<quint8*>(public_key.data()),
                                    &public_key_size) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_get_raw_public_key failed";
        return QByteArray();
    }

    if (public_key_size != static_cast<size_t>(public_key.size()))
    {
        LOG(ERROR) << "Unexpected public key size:" << public_key_size;
        return QByteArray();
    }

    return public_key;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray Signature::create(const SecureByteArray& private_key, const QByteArray& data)
{
    if (private_key.size() != kKeySize)
    {
        LOG(ERROR) << "Invalid private key size:" << private_key.size();
        return QByteArray();
    }

    EVP_PKEY_ptr pkey(EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const quint8*>(private_key.constData()),
        static_cast<size_t>(private_key.size())));
    if (!pkey)
    {
        LOG(ERROR) << "EVP_PKEY_new_raw_private_key failed";
        return QByteArray();
    }

    EVP_MD_CTX_ptr context(EVP_MD_CTX_new());
    if (!context)
    {
        LOG(ERROR) << "EVP_MD_CTX_new failed";
        return QByteArray();
    }

    if (EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, pkey.get()) != 1)
    {
        LOG(ERROR) << "EVP_DigestSignInit failed";
        return QByteArray();
    }

    QByteArray signature;
    signature.resize(kSignatureSize);

    size_t signature_size = static_cast<size_t>(signature.size());

    if (EVP_DigestSign(context.get(), reinterpret_cast<quint8*>(signature.data()), &signature_size,
                       reinterpret_cast<const quint8*>(data.constData()),
                       static_cast<size_t>(data.size())) != 1)
    {
        LOG(ERROR) << "EVP_DigestSign failed";
        return QByteArray();
    }

    if (signature_size != static_cast<size_t>(signature.size()))
    {
        LOG(ERROR) << "Unexpected signature size:" << signature_size;
        return QByteArray();
    }

    return signature;
}

//--------------------------------------------------------------------------------------------------
// static
bool Signature::verify(QByteArrayView public_key, QByteArrayView data, QByteArrayView signature)
{
    if (public_key.size() != kKeySize)
    {
        LOG(ERROR) << "Invalid public key size:" << public_key.size();
        return false;
    }

    if (signature.size() != kSignatureSize)
    {
        LOG(ERROR) << "Invalid signature size:" << signature.size();
        return false;
    }

    EVP_PKEY_ptr pkey(EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr, reinterpret_cast<const quint8*>(public_key.constData()),
        static_cast<size_t>(public_key.size())));
    if (!pkey)
    {
        LOG(ERROR) << "EVP_PKEY_new_raw_public_key failed";
        return false;
    }

    EVP_MD_CTX_ptr context(EVP_MD_CTX_new());
    if (!context)
    {
        LOG(ERROR) << "EVP_MD_CTX_new failed";
        return false;
    }

    if (EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, pkey.get()) != 1)
    {
        LOG(ERROR) << "EVP_DigestVerifyInit failed";
        return false;
    }

    return EVP_DigestVerify(context.get(),
                            reinterpret_cast<const quint8*>(signature.constData()),
                            static_cast<size_t>(signature.size()),
                            reinterpret_cast<const quint8*>(data.constData()),
                            static_cast<size_t>(data.size())) == 1;
}
