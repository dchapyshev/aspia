//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__ENDIAN_H
#define BASE__ENDIAN_H

#include "base/macros_magic.h"
#include "build/build_config.h"

#include <cstdint>

#if defined(CC_MSVC)
#include <intrin.h>
#endif // defined(CC_MSVC)

namespace base {

class Endian
{
public:
    static bool isLittle();
    static bool isBig() { return !isLittle(); }

    static uint16_t byteSwap(uint16_t value);
    static uint32_t byteSwap(uint32_t value);
    static uint64_t byteSwap(uint64_t value);

    template <typename T>
    static T toBig(T value)
    {
        if (isLittle())
            return byteSwap(value);

        return value;
    }

    template <typename T>
    static T fromBig(T value)
    {
        if (isLittle())
            return byteSwap(value);

        return value;
    }

    template <typename T>
    static T toLittle(T value)
    {
        if (isBig())
            return byteSwap(value);

        return value;
    }

    template <typename T>
    static T fromLittle(T value)
    {
        if (isBig())
            return byteSwap(value);

        return value;
    }

private:
    DISALLOW_COPY_AND_ASSIGN(Endian);
};

// static
bool Endian::isLittle()
{
    const union
    {
        long one;
        char little;
    } is_endian = { 1 };

    return is_endian.little;
}

// static
uint16_t Endian::byteSwap(uint16_t value)
{
#if defined(CC_MSVC)
    return _byteswap_ushort(value);
#else // defined(CC_MSVC)
    return ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
#endif // defined(CC_*)
}

// static
uint32_t Endian::byteSwap(uint32_t value)
{
#if defined(CC_MSVC)
    return _byteswap_ulong(value);
#else // defined(CC_MSVC)
    return ((value & 0x000000FFUL) << 24) |
           ((value & 0x0000FF00UL) << 8) |
           ((value & 0x00FF0000UL) >> 8) |
           ((value & 0xFF000000UL) >> 24);
#endif // defined(CC_*)
}

// static
uint64_t Endian::byteSwap(uint64_t value)
{
#if defined(CC_MSVC)
    return _byteswap_uint64(value);
#else // defined(CC_MSVC)
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
#endif // defined(CC_*)
}

} // namespace base

#endif // BASE__ENDIAN_H
