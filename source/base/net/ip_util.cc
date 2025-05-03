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

#include "base/net/ip_util.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <WS2tcpip.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#else
#error Platform support not implemented
#endif // defined(OS_*)

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool ipv4FromString(const QString& str, quint32* ip)
{
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, str.toLocal8Bit().data(), &(sa.sin_addr)) != 1)
        return false;

#if defined(OS_WIN)
    *ip = sa.sin_addr.S_un.S_addr;
#else
    *ip = sa.sin_addr.s_addr;
#endif

    *ip = htonl(*ip);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool isValidIpV4Address(const QString& address)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, address.toLocal8Bit().data(), &(sa.sin_addr)) != 0;
}

//--------------------------------------------------------------------------------------------------
bool isValidIpV6Address(const QString& address)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, address.toLocal8Bit().data(), &(sa.sin6_addr)) != 0;
}

//--------------------------------------------------------------------------------------------------
bool isIpInRange(const QString& ip, const QString& network, const QString& mask)
{
    quint32 ip_addr = 0;
    if (!ipv4FromString(ip, &ip_addr))
        return false;

    quint32 network_addr = 0;
    if (!ipv4FromString(network, &network_addr))
        return false;

    quint32 mask_addr = 0;
    if (!ipv4FromString(mask, &mask_addr))
        return false;

    quint32 lower_addr = (network_addr & mask_addr);
    quint32 upper_addr = (lower_addr | (~mask_addr));

    if (ip_addr >= lower_addr && ip_addr <= upper_addr)
        return true;

    return false;
}

} // namespace base
