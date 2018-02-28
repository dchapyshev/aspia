//
// PROJECT:         Aspia
// FILE:            crypto/random.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/random.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

std::string CreateRandomBuffer(size_t size)
{
    std::string buffer;
    buffer.resize(size);

    randombytes_buf(buffer.data(), buffer.size());

    return buffer;
}

uint32_t CreateRandomNumber()
{
    return randombytes_random();
}

} // namespace aspia
