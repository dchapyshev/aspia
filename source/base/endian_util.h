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

#ifndef BASE_ENDIAN_UTIL_H
#define BASE_ENDIAN_UTIL_H

#include "base/macros_magic.h"

#include <QtGlobal>

namespace base {

class EndianUtil
{
public:
    static bool isLittle();
    static bool isBig() { return !isLittle(); }

    static quint16 byteSwap(quint16 value);
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
    DISALLOW_COPY_AND_ASSIGN(EndianUtil);
};

} // namespace base

#endif // BASE_ENDIAN_UTIL_H
