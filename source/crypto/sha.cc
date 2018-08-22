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

#define SODIUM_STATIC
#include <sodium.h>

namespace aspia {

Sha256::Sha256()
{
    static_assert(sizeof(state_) == sizeof(crypto_hash_sha256_state));
    crypto_hash_sha256_init(reinterpret_cast<crypto_hash_sha256_state*>(&state_));
}

Sha256::~Sha256() = default;

// static
std::string Sha256::hash(const std::string& data)
{
    Sha256 sha;
    sha.addData(data);
    return sha.result();
}

// static
std::string Sha256::hash(const std::string& data, size_t rounds)
{
    uint8_t buffer[crypto_hash_sha256_BYTES];

    const uint8_t* data_buffer = reinterpret_cast<const uint8_t*>(data.c_str());
    size_t data_size = data.size();

    for (size_t i = 0; i < rounds; ++i)
    {
        crypto_hash_sha256(buffer, data_buffer, data_size);

        data_buffer = buffer;
        data_size = crypto_hash_sha256_BYTES;
    }

    std::string result;
    result.resize(crypto_hash_sha256_BYTES);

    memcpy(result.data(), buffer, crypto_hash_sha256_BYTES);
    sodium_memzero(buffer, sizeof(buffer));

    return result;
}

void Sha256::addData(const void* data, size_t length)
{
    crypto_hash_sha256_update(reinterpret_cast<crypto_hash_sha256_state*>(&state_),
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

    crypto_hash_sha256_final(reinterpret_cast<crypto_hash_sha256_state*>(&state_),
                             reinterpret_cast<uint8_t*>(data.data()));
    return data;
}

Sha512::Sha512()
{
    static_assert(sizeof(state_) == sizeof(crypto_hash_sha512_state));
    crypto_hash_sha512_init(reinterpret_cast<crypto_hash_sha512_state*>(state_));
}

Sha512::~Sha512() = default;

// static
std::string Sha512::hash(const std::string& data)
{
    Sha512 sha;
    sha.addData(data);
    return sha.result();
}

// static
std::string Sha512::hash(const std::string& data, size_t rounds)
{
    uint8_t buffer[crypto_hash_sha512_BYTES];

    const uint8_t* data_buffer = reinterpret_cast<const uint8_t*>(data.c_str());
    size_t data_size = data.size();

    for (size_t i = 0; i < rounds; ++i)
    {
        crypto_hash_sha512(buffer, data_buffer, data_size);

        data_buffer = buffer;
        data_size = crypto_hash_sha512_BYTES;
    }

    std::string result;
    result.resize(crypto_hash_sha512_BYTES);

    memcpy(result.data(), buffer, crypto_hash_sha512_BYTES);
    sodium_memzero(buffer, sizeof(buffer));

    return result;
}

void Sha512::addData(const void* data, size_t length)
{
    crypto_hash_sha512_update(reinterpret_cast<crypto_hash_sha512_state*>(state_),
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

    crypto_hash_sha512_final(reinterpret_cast<crypto_hash_sha512_state*>(state_),
                             reinterpret_cast<uint8_t*>(data.data()));
    return data;
}

} // namespace aspia
