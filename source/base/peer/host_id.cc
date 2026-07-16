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

#include "base/peer/host_id.h"

#include <limits>

const HostId kInvalidHostId = 0;
const HostId kAllHostsId = std::numeric_limits<HostId>::max();
const HostId kMinTempHostId = 900000000;
const HostId kMaxTempHostId = 999999999;

static_assert(sizeof(HostId) == 8);

//--------------------------------------------------------------------------------------------------
bool isTempHostId(HostId host_id)
{
    return host_id >= kMinTempHostId && host_id <= kMaxTempHostId;
}

//--------------------------------------------------------------------------------------------------
bool isHostId(const QString& str)
{
    if (str.isEmpty())
        return false;

    bool result = true;

    for (qsizetype i = 0; i < str.size(); ++i)
    {
        if (!str.at(i).isDigit())
        {
            result = false;
            break;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
HostId stringToHostId(const QString& str)
{
    if (str.isEmpty())
        return kInvalidHostId;

    bool ok = false;
    HostId host_id = str.toULongLong(&ok);
    if (!ok)
        return kInvalidHostId;

    return host_id;
}

//--------------------------------------------------------------------------------------------------
QString hostIdToString(HostId host_id)
{
    if (host_id == kInvalidHostId)
        return QString();

    return QString::number(host_id);
}
