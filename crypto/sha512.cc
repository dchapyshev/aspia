//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/sha512.h"
#include "base/logging.h"
#include "base/macros.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

namespace aspia {

bool CreateSHA512(const std::string& data, std::string& data_hash)
{
    data_hash.resize(crypto_hash_sha512_BYTES);

    if (crypto_hash_sha512(reinterpret_cast<uint8_t*>(&data_hash[0]),
                           reinterpret_cast<const uint8_t*>(data.data()),
                           data.size()) != 0)
    {
        LOG(ERROR) << "crypto_hash_sha512() failed";
        return false;
    }

    return true;
}

} // namespace aspia
