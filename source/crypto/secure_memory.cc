//
// PROJECT:         Aspia
// FILE:            crypto/secure_memory.cc
// LICENSE:         Mozilla Public License Version 2.0
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

void SecureMemZero(void* buffer, size_t size)
{
    if (!buffer || !size)
        return;

    sodium_memzero(buffer, size);
}

template<class StringType>
void SecureMemZeroT(StringType* str)
{
    if (!str)
        return;

    const size_t memory_size = str->length() * sizeof(StringType::value_type);

    if (!memory_size)
        return;

    sodium_memzero(str->data(), memory_size);
}

void SecureMemZero(std::string* str)
{
    SecureMemZeroT<std::string>(str);
}

void SecureMemZero(std::wstring* str)
{
    SecureMemZeroT<std::wstring>(str);
}

} // namespace aspia
