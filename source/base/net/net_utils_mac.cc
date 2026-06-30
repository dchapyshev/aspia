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

#include <QHostAddress>
#include <QtEndian>

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <net/route.h>
#include <netinet/in.h>

#include <vector>

namespace {

//--------------------------------------------------------------------------------------------------
QString sockaddrToIpv4(const sockaddr* address)
{
    if (!address || address->sa_family != AF_INET)
        return QString();

    const auto* in = reinterpret_cast<const sockaddr_in*>(address);
    return QHostAddress(qFromBigEndian<quint32>(in->sin_addr.s_addr)).toString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Route> NetUtils::routeTable()
{
    QList<Route> routes;

    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };

    size_t needed = 0;
    if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) < 0 || needed == 0)
        return routes;

    std::vector<char> buffer(needed);
    if (sysctl(mib, 6, buffer.data(), &needed, nullptr, 0) < 0)
        return routes;

    char* end = buffer.data() + needed;
    for (char* next = buffer.data(); next < end;)
    {
        auto* message = reinterpret_cast<rt_msghdr*>(next);
        next += message->rtm_msglen;

        // The sockaddr array follows the header; present entries are selected by the rtm_addrs mask.
        auto* address = reinterpret_cast<sockaddr*>(message + 1);
        sockaddr* table[RTAX_MAX] = {};
        for (int i = 0; i < RTAX_MAX; ++i)
        {
            if (!(message->rtm_addrs & (1 << i)))
                continue;

            table[i] = address;
            const size_t len = address->sa_len ? address->sa_len : sizeof(quint32);
            const size_t step = (len + sizeof(quint32) - 1) & ~(sizeof(quint32) - 1);
            address = reinterpret_cast<sockaddr*>(reinterpret_cast<char*>(address) + step);
        }

        if (!table[RTAX_DST] || table[RTAX_DST]->sa_family != AF_INET)
            continue;

        // Metric is not exposed by the routing socket, so it stays 0.
        routes.emplaceBack(sockaddrToIpv4(table[RTAX_DST]), sockaddrToIpv4(table[RTAX_NETMASK]),
                           sockaddrToIpv4(table[RTAX_GATEWAY]), quint32(0));
    }

    return routes;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Adapter> NetUtils::adapters()
{
    // Not implemented on macOS.
    return QList<NetUtils::Adapter>();
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Connection> NetUtils::connections()
{
    // Not implemented on macOS.
    return QList<NetUtils::Connection>();
}
