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

} // namespace base

#endif // BASE__ENDIAN_H
