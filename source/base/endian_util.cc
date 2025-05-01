//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/endian_util.h"

#include "build/build_config.h"

#include <bit>

#if defined(CC_MSVC)
#include <intrin.h>
#endif // defined(CC_MSVC)

namespace base {

//--------------------------------------------------------------------------------------------------
// static
bool EndianUtil::isLittle()
{
    return std::endian::native == std::endian::little;
}

//--------------------------------------------------------------------------------------------------
// static
quint16 EndianUtil::byteSwap(quint16 value)
{
#if defined(CC_MSVC)
    return _byteswap_ushort(value);
#elif defined(CC_GCC)
    return __builtin_bswap16(value);
#else
    return (((value >> 8) & 0xFFu) | ((value & 0xFFu) << 8));
#endif // defined(CC_*)
}

//--------------------------------------------------------------------------------------------------
// static
uint32_t EndianUtil::byteSwap(uint32_t value)
{
#if defined(CC_MSVC)
    return _byteswap_ulong(value);
#elif defined(CC_GCC)
    return __builtin_bswap32(value);
#else
    return (((value & 0xFF000000u) >> 24) |
            ((value & 0x00FF0000u) >> 8) |
            ((value & 0x0000FF00u) << 8) |
            ((value & 0x000000FFu) << 24));
#endif // defined(CC_*)
}

//--------------------------------------------------------------------------------------------------
// static
uint64_t EndianUtil::byteSwap(uint64_t value)
{
#if defined(CC_MSVC)
    return _byteswap_uint64(value);
#elif defined(CC_GCC)
    return __builtin_bswap64(value);
#else
    return (((value & 0xFF00000000000000ull) >> 56) |
            ((value & 0x00FF000000000000ull) >> 40) |
            ((value & 0x0000FF0000000000ull) >> 24) |
            ((value & 0x000000FF00000000ull) >> 8) |
            ((value & 0x00000000FF000000ull) << 8) |
            ((value & 0x0000000000FF0000ull) << 24) |
            ((value & 0x000000000000FF00ull) << 40) |
            ((value & 0x00000000000000FFull) << 56));
#endif // defined(CC_*)
}

} // namespace base
