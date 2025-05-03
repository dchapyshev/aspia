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

#include "base/crypto/key_pair.h"

#include "base/logging.h"

#include <openssl/evp.h>

namespace base {

//--------------------------------------------------------------------------------------------------
KeyPair::KeyPair() = default;

//--------------------------------------------------------------------------------------------------
KeyPair::KeyPair(EVP_PKEY_ptr&& pkey) noexcept
    : pkey_(std::move(pkey))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
KeyPair::KeyPair(KeyPair&& other) noexcept
    : pkey_(std::move(other.pkey_))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
KeyPair& KeyPair::operator=(KeyPair&& other) noexcept
{
    if (&other != this)
        pkey_ = std::move(other.pkey_);

    return *this;
}

//--------------------------------------------------------------------------------------------------
KeyPair::~KeyPair() = default;

//--------------------------------------------------------------------------------------------------
// static
KeyPair KeyPair::create(Type type)
{
    DCHECK_EQ(type, Type::X25519);

    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    if (!ctx)
    {
        LOG(LS_ERROR) << "EVP_PKEY_CTX_new_id failed";
        return KeyPair();
    }

    if (EVP_PKEY_keygen_init(ctx.get()) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_keygen_init failed";
        return KeyPair();
    }

    EVP_PKEY* private_key = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &private_key) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_keygen failed";
        return KeyPair();
    }

    return KeyPair(EVP_PKEY_ptr(private_key));
}

//--------------------------------------------------------------------------------------------------
// static
KeyPair KeyPair::fromPrivateKey(const QByteArray& private_key)
{
    if (private_key.isEmpty())
    {
        LOG(LS_ERROR) << "Empty private key";
        return KeyPair();
    }

    return KeyPair(EVP_PKEY_ptr(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const quint8*>(private_key.data()), private_key.size())));
}

//--------------------------------------------------------------------------------------------------
bool KeyPair::isValid() const
{
    return pkey_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
QByteArray KeyPair::privateKey() const
{
    size_t private_key_length = 0;

    if (EVP_PKEY_get_raw_private_key(pkey_.get(), nullptr, &private_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_get_raw_private_key failed";
        return QByteArray();
    }

    if (!private_key_length)
    {
        LOG(LS_ERROR) << "Invalid private key size";
        return QByteArray();
    }

    QByteArray private_key;
    private_key.resize(static_cast<QByteArray::size_type>(private_key_length));

    if (EVP_PKEY_get_raw_private_key(pkey_.get(), reinterpret_cast<quint8*>(private_key.data()),
        &private_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_get_raw_private_key failed";
        return QByteArray();
    }

    if (private_key.size() != private_key_length)
    {
        LOG(LS_ERROR) << "Invalid private key size";
        return QByteArray();
    }

    return private_key;
}

//--------------------------------------------------------------------------------------------------
QByteArray KeyPair::publicKey() const
{
    size_t public_key_length = 0;

    if (EVP_PKEY_get_raw_public_key(pkey_.get(), nullptr, &public_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_get_raw_public_key failed";
        return QByteArray();
    }

    if (!public_key_length)
    {
        LOG(LS_ERROR) << "Invalid public key size";
        return QByteArray();
    }

    QByteArray public_key;
    public_key.resize(static_cast<QByteArray::size_type>(public_key_length));

    if (EVP_PKEY_get_raw_public_key(pkey_.get(), reinterpret_cast<quint8*>(public_key.data()),
        &public_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_get_raw_public_key failed";
        return QByteArray();
    }

    if (public_key.size() != public_key_length)
    {
        LOG(LS_ERROR) << "Invalid public key size";
        return QByteArray();
    }

    return public_key;
}

//--------------------------------------------------------------------------------------------------
QByteArray KeyPair::sessionKey(const QByteArray& peer_public_key) const
{
    if (peer_public_key.isEmpty())
    {
        LOG(LS_ERROR) << "Empty peer public key";
        return QByteArray();
    }

    EVP_PKEY_ptr public_key(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
        reinterpret_cast<const quint8*>(peer_public_key.data()), peer_public_key.size()));
    if (!public_key)
    {
        LOG(LS_ERROR) << "EVP_PKEY_new_raw_public_key failed";
        return QByteArray();
    }

    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new(pkey_.get(), nullptr));
    if (!ctx)
    {
        LOG(LS_ERROR) << "EVP_PKEY_CTX_new failed";
        return QByteArray();
    }

    if (EVP_PKEY_derive_init(ctx.get()) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_derive_init failed";
        return QByteArray();
    }

    if (EVP_PKEY_derive_set_peer(ctx.get(), public_key.get()) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_derive_set_pee failed";
        return QByteArray();
    }

    size_t session_key_length = 0;

    if (EVP_PKEY_derive(ctx.get(), nullptr, &session_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_derive failed";
        return QByteArray();
    }

    if (!session_key_length)
    {
        LOG(LS_ERROR) << "Invalid session key size";
        return QByteArray();
    }

    QByteArray session_key;
    session_key.resize(static_cast<QByteArray::size_type>(session_key_length));

    if (EVP_PKEY_derive(ctx.get(), reinterpret_cast<quint8*>(session_key.data()),
        &session_key_length) != 1)
    {
        LOG(LS_ERROR) << "EVP_PKEY_derive failed";
        return QByteArray();
    }

    if (session_key.size() != session_key_length)
    {
        LOG(LS_ERROR) << "Invalid session key size";
        return QByteArray();
    }

    return session_key;
}

} // namespace base
