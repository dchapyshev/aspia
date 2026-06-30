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

#include "base/net/net_utils.h"

#include <QByteArray>
#include <QFile>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Converts an in_addr.s_addr stored as host-order hex (the form used in /proc/net/route) to a
// dotted IPv4 string.
QString hexToIpv4(const QByteArray& hex)
{
    bool ok = false;
    const quint32 value = hex.toUInt(&ok, 16);
    if (!ok)
        return QString();

    return QString("%1.%2.%3.%4")
        .arg(value & 0xFF).arg((value >> 8) & 0xFF).arg((value >> 16) & 0xFF).arg((value >> 24) & 0xFF);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Route> NetUtils::routeTable()
{
    QList<Route> routes;

    // /proc/net/route lists routes in tab-separated fields: Iface Destination Gateway Flags RefCnt
    // Use Metric Mask MTU Window IRTT. Addresses are host-order hex of in_addr.s_addr.
    QFile file("/proc/net/route");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return routes;

    file.readLine(); // Skip the header line.

    while (!file.atEnd())
    {
        const QList<QByteArray> fields = file.readLine().simplified().split(' ');
        if (fields.size() < 8)
            continue;

        // Fields: destination[1], gateway[2], metric[6], mask[7].
        routes.emplaceBack(hexToIpv4(fields[1]), hexToIpv4(fields[7]),
                           hexToIpv4(fields[2]), fields[6].toUInt());
    }

    return routes;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Adapter> NetUtils::adapters()
{
    // Adapter enumeration is a host-side feature and is not used by the Android client.
    NOTIMPLEMENTED();
    return QList<Adapter>();
}
