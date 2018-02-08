//
// PROJECT:         Aspia
// FILE:            crypto/random.cc
// LICENSE:         Mozilla Public License Version 2.0
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
