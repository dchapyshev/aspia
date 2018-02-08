//
// PROJECT:         Aspia
// FILE:            crypto/sha.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/sha.h"
#include "base/logging.h"

namespace aspia {

StreamSHA512::StreamSHA512()
{
    crypto_hash_sha512_init(&state_);
}

void StreamSHA512::AppendData(const void* data, size_t size)
{
#ifndef NDEBUG
    DCHECK(!final_called_);
#endif // NDEBUG

    crypto_hash_sha512_update(&state_, reinterpret_cast<const uint8_t*>(data), size);
}

void StreamSHA512::AppendData(const std::string& data)
{
    AppendData(data.c_str(), data.length());
}

void StreamSHA512::AppendString(const std::wstring& string)
{
    AppendData(string.c_str(), string.length() * sizeof(std::wstring::value_type));
}

std::string StreamSHA512::Final()
{
#ifndef NDEBUG
    final_called_ = true;
#endif // NDEBUG

    std::string hash;
    hash.resize(crypto_hash_sha512_BYTES);

    crypto_hash_sha512_final(&state_, reinterpret_cast<uint8_t*>(hash.data()));

    return hash;
}

std::string SHA512(std::wstring_view data, size_t iter_count)
{
    DCHECK_NE(iter_count, 0);

    uint8_t hash_buffer[crypto_hash_sha512_BYTES];
    sodium_memzero(hash_buffer, sizeof(hash_buffer));

    size_t source_buffer_size = data.length() * sizeof(std::wstring_view::value_type);
    const uint8_t* source_buffer = reinterpret_cast<const uint8_t*>(data.data());

    for (size_t i = 0; i < iter_count; ++i)
    {
        crypto_hash_sha512(hash_buffer, source_buffer, source_buffer_size);

        source_buffer_size = crypto_hash_sha512_BYTES;
        source_buffer = hash_buffer;
    }

    std::string data_hash;
    data_hash.resize(crypto_hash_sha512_BYTES);
    memcpy(data_hash.data(), hash_buffer, crypto_hash_sha512_BYTES);

    sodium_memzero(hash_buffer, crypto_hash_sha512_BYTES);

    return data_hash;
}

std::string SHA256(std::wstring_view data, size_t iter_count)
{
    DCHECK_NE(iter_count, 0);

    uint8_t hash_buffer[crypto_hash_sha256_BYTES];
    sodium_memzero(hash_buffer, sizeof(hash_buffer));

    size_t source_buffer_size = data.length() * sizeof(std::wstring::value_type);
    const uint8_t* source_buffer = reinterpret_cast<const uint8_t*>(data.data());

    for (size_t i = 0; i < iter_count; ++i)
    {
        crypto_hash_sha256(hash_buffer, source_buffer, source_buffer_size);

        source_buffer_size = crypto_hash_sha256_BYTES;
        source_buffer = hash_buffer;
    }

    std::string data_hash;
    data_hash.resize(crypto_hash_sha256_BYTES);
    memcpy(data_hash.data(), hash_buffer, crypto_hash_sha256_BYTES);

    sodium_memzero(hash_buffer, crypto_hash_sha256_BYTES);

    return data_hash;
}

} // namespace aspia
