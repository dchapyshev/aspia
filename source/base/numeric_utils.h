//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NUMERIC_UTILS_H
#define BASE_NUMERIC_UTILS_H

#include <QtGlobal>

namespace base {

//--------------------------------------------------------------------------------------------------
inline quint32 makeUint32(quint16 high, quint16 low)
{
    return (static_cast<quint32>(high) << 16) | static_cast<quint32>(low);
}

//--------------------------------------------------------------------------------------------------
inline quint16 highWord(quint32 value)
{
    return static_cast<quint16>(value >> 16);
}

//--------------------------------------------------------------------------------------------------
inline quint16 lowWord(quint32 value)
{
    return static_cast<quint16>(value & 0xFFFF);
}

} // namespace base

#endif // BASE_NUMERIC_UTILS_H
