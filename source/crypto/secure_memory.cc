//
// PROJECT:         Aspia
// FILE:            crypto/secure_memory.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/secure_memory.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

void secureMemZero(void* data, size_t data_size)
{
    if (data && data_size)
        sodium_memzero(data, data_size);
}

void secureMemZero(std::string* str)
{
    if (!str)
        return;

    secureMemZero(str->data(), str->size());
}

void secureMemZero(QString* str)
{
    if (!str)
        return;

    secureMemZero(str->data(), str->size());
}

void secureMemZero(QByteArray* bytes)
{
    if (!bytes)
        return;

    secureMemZero(bytes->data(), bytes->size());
}

} // namespace aspia
