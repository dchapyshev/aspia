//
// PROJECT:         Aspia
// FILE:            crypto/random.cc
// LICENSE:         GNU General Public License 3
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

// static
QByteArray Random::generateBuffer(int size)
{
    QByteArray random_buffer;
    random_buffer.resize(size);

    randombytes_buf(random_buffer.data(), random_buffer.size());

    return random_buffer;
}

// static
quint32 Random::generateNumber()
{
    return randombytes_random();
}

} // namespace aspia
