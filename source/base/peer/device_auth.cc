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

#include "base/peer/device_auth.h"

#include <openssl/evp.h>

#include "base/logging.h"
#include "base/crypto/openssl_util.h"
#include "base/crypto/random.h"

namespace {

//--------------------------------------------------------------------------------------------------
QByteArray challengeMessage(const QByteArray& token_id, const QByteArray& server_nonce)
{
    QByteArray msg;
    msg.reserve(token_id.size() + server_nonce.size());
    msg.append(token_id);
    msg.append(server_nonce);
    return msg;
}

//--------------------------------------------------------------------------------------------------
EVP_PKEY_ptr loadPrivateKey(const SecureByteArray& private_key)
{
    if (private_key.size() != DeviceAuth::kPrivateKeySize)
    {
        LOG(ERROR) << "Invalid Ed25519 private key size: " << private_key.size();
        return EVP_PKEY_ptr();
    }
    return EVP_PKEY_ptr(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const quint8*>(private_key.constData()), private_key.size()));
}

//--------------------------------------------------------------------------------------------------
EVP_PKEY_ptr loadPublicKey(const QByteArray& public_key)
{
    if (public_key.size() != DeviceAuth::kPublicKeySize)
    {
        LOG(ERROR) << "Invalid Ed25519 public key size: " << public_key.size();
        return EVP_PKEY_ptr();
    }
    return EVP_PKEY_ptr(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const quint8*>(public_key.constData()), public_key.size()));
}

//--------------------------------------------------------------------------------------------------
QByteArray extractRawPublicKey(EVP_PKEY* pkey)
{
    size_t public_key_length = 0;
    if (EVP_PKEY_get_raw_public_key(pkey, nullptr, &public_key_length) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_get_raw_public_key failed (size query)";
        return QByteArray();
    }
    if (public_key_length != DeviceAuth::kPublicKeySize)
    {
        LOG(ERROR) << "Unexpected Ed25519 public key length: " << public_key_length;
        return QByteArray();
    }

    QByteArray public_key;
    public_key.resize(static_cast<qsizetype>(public_key_length));
    if (EVP_PKEY_get_raw_public_key(pkey,
        reinterpret_cast<quint8*>(public_key.data()), &public_key_length) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_get_raw_public_key failed";
        return QByteArray();
    }
    return public_key;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray extractRawPrivateKey(EVP_PKEY* pkey)
{
    size_t private_key_length = 0;
    if (EVP_PKEY_get_raw_private_key(pkey, nullptr, &private_key_length) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_get_raw_private_key failed (size query)";
        return SecureByteArray();
    }
    if (private_key_length != DeviceAuth::kPrivateKeySize)
    {
        LOG(ERROR) << "Unexpected Ed25519 private key length: " << private_key_length;
        return SecureByteArray();
    }

    QByteArray private_key;
    private_key.resize(static_cast<qsizetype>(private_key_length));
    if (EVP_PKEY_get_raw_private_key(pkey,
        reinterpret_cast<quint8*>(private_key.data()), &private_key_length) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_get_raw_private_key failed";
        return SecureByteArray();
    }
    return SecureByteArray(std::move(private_key));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
DeviceAuth::DeviceKey DeviceAuth::generateDeviceKey()
{
    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
    if (!ctx)
    {
        LOG(ERROR) << "EVP_PKEY_CTX_new_id failed";
        return DeviceKey();
    }
    if (EVP_PKEY_keygen_init(ctx.get()) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_keygen_init failed";
        return DeviceKey();
    }

    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw_pkey) != 1)
    {
        LOG(ERROR) << "EVP_PKEY_keygen failed";
        return DeviceKey();
    }
    EVP_PKEY_ptr pkey(raw_pkey);

    DeviceKey result;
    result.private_key = extractRawPrivateKey(pkey.get());
    result.public_key = extractRawPublicKey(pkey.get());
    if (!result.isValid())
        return DeviceKey();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray DeviceAuth::derivePublicKey(const SecureByteArray& private_key)
{
    EVP_PKEY_ptr pkey = loadPrivateKey(private_key);
    if (!pkey)
        return QByteArray();
    return extractRawPublicKey(pkey.get());
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray DeviceAuth::generateServerNonce()
{
    return Random::byteArray(kServerNonceSize);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray DeviceAuth::signChallenge(const SecureByteArray& private_key,
    const QByteArray& token_id, const QByteArray& server_nonce)
{
    if (token_id.size() != kTokenIdSize || server_nonce.size() != kServerNonceSize)
    {
        LOG(ERROR) << "Invalid challenge input sizes";
        return QByteArray();
    }

    EVP_PKEY_ptr pkey = loadPrivateKey(private_key);
    if (!pkey)
        return QByteArray();

    EVP_MD_CTX_ptr md_ctx(EVP_MD_CTX_new());
    if (!md_ctx)
    {
        LOG(ERROR) << "EVP_MD_CTX_new failed";
        return QByteArray();
    }

    if (EVP_DigestSignInit(md_ctx.get(), nullptr, nullptr, nullptr, pkey.get()) != 1)
    {
        LOG(ERROR) << "EVP_DigestSignInit failed";
        return QByteArray();
    }

    const QByteArray message = challengeMessage(token_id, server_nonce);

    size_t sig_len = kSignatureSize;
    QByteArray signature;
    signature.resize(static_cast<qsizetype>(sig_len));
    if (EVP_DigestSign(md_ctx.get(), reinterpret_cast<quint8*>(signature.data()), &sig_len,
        reinterpret_cast<const quint8*>(message.constData()), message.size()) != 1)
    {
        LOG(ERROR) << "EVP_DigestSign failed";
        return QByteArray();
    }
    signature.resize(static_cast<qsizetype>(sig_len));
    return signature;
}

//--------------------------------------------------------------------------------------------------
// static
bool DeviceAuth::verifyChallenge(const QByteArray& public_key,
    const QByteArray& token_id, const QByteArray& server_nonce, const QByteArray& signature)
{
    if (token_id.size() != kTokenIdSize || server_nonce.size() != kServerNonceSize
        || signature.size() != kSignatureSize)
    {
        return false;
    }

    EVP_PKEY_ptr pkey = loadPublicKey(public_key);
    if (!pkey)
        return false;

    EVP_MD_CTX_ptr md_ctx(EVP_MD_CTX_new());
    if (!md_ctx)
    {
        LOG(ERROR) << "EVP_MD_CTX_new failed";
        return false;
    }

    if (EVP_DigestVerifyInit(md_ctx.get(), nullptr, nullptr, nullptr, pkey.get()) != 1)
    {
        LOG(ERROR) << "EVP_DigestVerifyInit failed";
        return false;
    }

    const QByteArray message = challengeMessage(token_id, server_nonce);

    return EVP_DigestVerify(md_ctx.get(),
        reinterpret_cast<const quint8*>(signature.constData()), signature.size(),
        reinterpret_cast<const quint8*>(message.constData()), message.size()) == 1;
}
