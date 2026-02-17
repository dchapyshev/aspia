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

#include "base/net/route_enumerator.h"

#include <qt_windows.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <WinSock2.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QString ipToString(DWORD ip)
{
    char buffer[46 + 1];

    if (!inet_ntop(AF_INET, &ip, buffer, _countof(buffer)))
        return QString();

    return buffer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouteEnumerator::RouteEnumerator()
{
    ULONG buffer_size = sizeof(MIB_IPFORWARDTABLE);

    QByteArray forward_table_buffer;
    forward_table_buffer.resize(buffer_size);

    PMIB_IPFORWARDTABLE forward_table =
        reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer.data());

    DWORD error_code = GetIpForwardTable(forward_table, &buffer_size, 0);
    if (error_code != NO_ERROR)
    {
        if (error_code == ERROR_INSUFFICIENT_BUFFER)
        {
            forward_table_buffer.resize(buffer_size);
            forward_table = reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer.data());

            error_code = GetIpForwardTable(forward_table, &buffer_size, 0);
        }
    }

    if (error_code != NO_ERROR)
        return;

    forward_table_buffer_ = std::move(forward_table_buffer);
    num_entries_ = forward_table->dwNumEntries;
}

//--------------------------------------------------------------------------------------------------
RouteEnumerator::~RouteEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool RouteEnumerator::isAtEnd() const
{
    return pos_ >= num_entries_;
}

//--------------------------------------------------------------------------------------------------
void RouteEnumerator::advance()
{
    ++pos_;
}

//--------------------------------------------------------------------------------------------------
QString RouteEnumerator::destonation() const
{
    const MIB_IPFORWARDTABLE* forward_table =
        reinterpret_cast<const MIB_IPFORWARDTABLE*>(forward_table_buffer_.data());
    if (!forward_table)
        return QString();

    return ipToString(forward_table->table[pos_].dwForwardDest);
}

//--------------------------------------------------------------------------------------------------
QString RouteEnumerator::mask() const
{
    const MIB_IPFORWARDTABLE* forward_table =
        reinterpret_cast<const MIB_IPFORWARDTABLE*>(forward_table_buffer_.data());
    if (!forward_table)
        return QString();

    return ipToString(forward_table->table[pos_].dwForwardMask);
}

//--------------------------------------------------------------------------------------------------
QString RouteEnumerator::gateway() const
{
    const MIB_IPFORWARDTABLE* forward_table =
        reinterpret_cast<const MIB_IPFORWARDTABLE*>(forward_table_buffer_.data());
    if (!forward_table)
        return QString();

    return ipToString(forward_table->table[pos_].dwForwardNextHop);
}

//--------------------------------------------------------------------------------------------------
quint32 RouteEnumerator::metric() const
{
    const MIB_IPFORWARDTABLE* forward_table =
        reinterpret_cast<const MIB_IPFORWARDTABLE*>(forward_table_buffer_.data());
    if (!forward_table)
        return 0;

    return forward_table->table[pos_].dwForwardMetric1;
}

} // namespace base
