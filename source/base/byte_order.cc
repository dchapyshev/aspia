//
// PROJECT:         Aspia
// FILE:            base/byte_order.cc
// LICENSE:         GNU Lesser General Public License 2.1
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
    return _byteswap_ushort(value);
}

uint32_t ByteSwap(uint32_t value)
{
    return _byteswap_ulong(value);
}

uint64_t ByteSwap(uint64_t value)
{
    return _byteswap_uint64(value);
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
