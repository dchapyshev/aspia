//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/peer/host_id.h"

#include "base/strings/string_number_conversions.h"

#include <cctype>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <class T>
bool isHostIdT(T str)
{
    bool result = true;

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (!std::isdigit(static_cast<uint8_t>(str[i])))
        {
            result = false;
            break;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
template <class T>
HostId stringToHostIdT(T str)
{
    if (str.empty())
        return kInvalidHostId;

    HostId host_id;
    if (!stringToULong64(str, &host_id))
        return kInvalidHostId;

    return host_id;
}

} // namespace

const HostId kInvalidHostId = 0;

static_assert(sizeof(HostId) == 8);

//--------------------------------------------------------------------------------------------------
bool isHostId(std::u16string_view str)
{
    return isHostIdT(str);
}

//--------------------------------------------------------------------------------------------------
bool isHostId(std::string_view str)
{
    return isHostIdT(str);
}

//--------------------------------------------------------------------------------------------------
HostId stringToHostId(std::u16string_view str)
{
    return stringToHostIdT(str);
}

//--------------------------------------------------------------------------------------------------
HostId stringToHostId(std::string_view str)
{
    return stringToHostIdT(str);
}

//--------------------------------------------------------------------------------------------------
std::u16string hostIdToString16(HostId host_id)
{
    if (host_id == kInvalidHostId)
        return std::u16string();

    return numberToString16(host_id);
}

//--------------------------------------------------------------------------------------------------
std::string hostIdToString(HostId host_id)
{
    if (host_id == kInvalidHostId)
        return std::string();

    return numberToString(host_id);
}

} // namespace base
