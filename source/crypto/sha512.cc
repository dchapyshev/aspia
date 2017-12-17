//
// PROJECT:         Aspia
// FILE:            crypto/sha512.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/sha512.h"
#include "base/logging.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

bool CreateSHA512(const std::string& data, std::string& data_hash, size_t iter_count)
{
    if (!iter_count)
        return false;

    uint8_t hash_buffer[crypto_hash_sha512_BYTES];

    sodium_memzero(hash_buffer, sizeof(hash_buffer));

    size_t source_buffer_size = data.size();
    const uint8_t* source_buffer = reinterpret_cast<const uint8_t*>(data.data());

    for (size_t i = 0; i < iter_count; ++i)
    {
        if (crypto_hash_sha512(hash_buffer, source_buffer, source_buffer_size) != 0)
        {
            LOG(ERROR) << "crypto_hash_sha512() failed";
            return false;
        }

        source_buffer_size = crypto_hash_sha512_BYTES;
        source_buffer = hash_buffer;
    }

    data_hash.resize(crypto_hash_sha512_BYTES);
    memcpy(&data_hash[0], hash_buffer, crypto_hash_sha512_BYTES);

    sodium_memzero(hash_buffer, crypto_hash_sha512_BYTES);

    return true;
}

} // namespace aspia
