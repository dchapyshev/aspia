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

#include "crypto/sha.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

Sha256::Sha256()
{
    state_ = std::make_unique<crypto_hash_sha256_state>();
    crypto_hash_sha256_init(state_.get());
}

Sha256::~Sha256() = default;

// static
std::string Sha256::hash(const std::string& data)
{
    Sha256 sha;
    sha.addData(data);
    return sha.result();
}

void Sha256::addData(const void* data, size_t length)
{
    crypto_hash_sha256_update(state_.get(),
                              reinterpret_cast<const uint8_t*>(data),
                              length);
}

void Sha256::addData(const std::string& data)
{
    addData(data.c_str(), data.size());
}

std::string Sha256::result() const
{
    std::string data;
    data.resize(crypto_hash_sha256_BYTES);

    crypto_hash_sha256_final(state_.get(), reinterpret_cast<uint8_t*>(data.data()));
    return data;
}

Sha512::Sha512()
{
    state_ = std::make_unique<crypto_hash_sha512_state>();
    crypto_hash_sha512_init(state_.get());
}

Sha512::~Sha512() = default;

// static
std::string Sha512::hash(const std::string& data)
{
    Sha512 sha;
    sha.addData(data);
    return sha.result();
}

void Sha512::addData(const void* data, size_t length)
{
    crypto_hash_sha512_update(state_.get(),
                              reinterpret_cast<const uint8_t*>(data),
                              length);
}

void Sha512::addData(const std::string& data)
{
    addData(data.c_str(), data.size());
}

std::string Sha512::result() const
{
    std::string data;
    data.resize(crypto_hash_sha512_BYTES);

    crypto_hash_sha512_final(state_.get(), reinterpret_cast<uint8_t*>(data.data()));
    return data;
}

} // namespace aspia
