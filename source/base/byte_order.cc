//
// PROJECT:         Aspia
// FILE:            base/byte_order.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/byte_order.h"
#include "base/logging.h"

#include <algorithm>

#if (COMPILER_MSVC == 1)
#include <intrin.h>
#endif

namespace aspia {

uint16_t ByteSwap(uint16_t value)
{
#if (COMPILER_MSVC == 1)
    return _byteswap_ushort(value);
#else
    return ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
#endif
}

uint32_t ByteSwap(uint32_t value)
{
#if (COMPILER_MSVC == 1)
    return _byteswap_ulong(value);
#else
    return ((value & 0x000000FFUL) << 24) |
           ((value & 0x0000FF00UL) << 8) |
           ((value & 0x00FF0000UL) >> 8) |
           ((value & 0xFF000000UL) >> 24);
#endif
}

uint64_t ByteSwap(uint64_t value)
{
#if (COMPILER_MSVC == 1)
    return _byteswap_uint64(value);
#else
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
#endif
}

void ChangeByteOrder(uint8_t* data, size_t data_size)
{
    DCHECK(data != nullptr);
    DCHECK(data_size != 0);

    size_t i = 0;

    while (i < data_size)
    {
        const char temp = data[i];

        data[i] = data[i + 1];
        data[i + 1] = temp;

        i += 2;
    }
}

void ChangeByteOrder(char* data, size_t data_size)
{
    ChangeByteOrder(reinterpret_cast<uint8_t*>(data), data_size);
    data[data_size - 1] = 0;
}

} // namespace aspia
