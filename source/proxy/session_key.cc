//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "proxy/session_key.h"

#include "crypto/random.h"

namespace proxy {

SessionKey::SessionKey() = default;

SessionKey::SessionKey(crypto::KeyPair&& key_pair, base::ByteArray&& iv)
    : key_pair_(std::move(key_pair)),
      iv_(std::move(iv))
{
    // Nothing
}

SessionKey::SessionKey(SessionKey&& other) noexcept
    : key_pair_(std::move(other.key_pair_)),
      iv_(std::move(other.iv_))
{
    // Nothing
}

SessionKey& SessionKey::operator=(SessionKey&& other) noexcept
{
    if (&other != this)
    {
        key_pair_ = std::move(other.key_pair_);
        iv_ = std::move(other.iv_);
    }

    return *this;
}

SessionKey::~SessionKey() = default;

// static
SessionKey SessionKey::create()
{
    crypto::KeyPair key_pair = crypto::KeyPair::create(crypto::KeyPair::Type::X25519);
    if (!key_pair.isValid())
        return SessionKey();

    base::ByteArray iv = crypto::Random::byteArray(12);
    if (iv.empty())
        return SessionKey();

    return SessionKey(std::move(key_pair), std::move(iv));
}

bool SessionKey::isValid() const
{
    return key_pair_.isValid() && !iv_.empty();
}

base::ByteArray SessionKey::privateKey() const
{
    return key_pair_.privateKey();
}

base::ByteArray SessionKey::publicKey() const
{
    return key_pair_.publicKey();
}

base::ByteArray SessionKey::sessionKey(const base::ByteArray& peer_public_key)
{
    return key_pair_.sessionKey(peer_public_key);
}

base::ByteArray SessionKey::iv() const
{
    return iv_;
}

} // namespace proxy
