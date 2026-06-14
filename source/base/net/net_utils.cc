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
#include <QNetworkInterface>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <QFile>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include <QtEndian>

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <net/route.h>
#include <netinet/in.h>
#endif // defined(Q_OS_MACOS)

#include <limits>
#include <vector>

namespace {

const int kMaxHostNameLength = 64;

//--------------------------------------------------------------------------------------------------
bool isValidHostNameChar(const QChar c)
{
    if (c.isLetterOrNumber())
        return true;

    if (c == '.' || c == '_' || c == '-')
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_WINDOWS)
QString ipToString(DWORD ip)
{
    char buffer[46 + 1];

    if (!inet_ntop(AF_INET, &ip, buffer, _countof(buffer)))
        return QString();

    return buffer;
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_LINUX)
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
#endif // defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_MACOS)
QString sockaddrToIpv4(const sockaddr* address)
{
    if (!address || address->sa_family != AF_INET)
        return QString();

    const auto* in = reinterpret_cast<const sockaddr_in*>(address);
    return QHostAddress(qFromBigEndian<quint32>(in->sin_addr.s_addr)).toString();
}
#endif // defined(Q_OS_MACOS)

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QStringList NetUtils::localIpList()
{
    QStringList result;

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : std::as_const(interfaces))
    {
        QNetworkInterface::InterfaceFlags flags = iface.flags();

        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning))
            continue;

        if (flags & QNetworkInterface::IsLoopBack)
            continue;

        QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : std::as_const(entries))
        {
            const QHostAddress& addr = entry.ip();
            if (addr.isNull() || !addr.isGlobal())
                continue;

            result.emplace_back(addr.toString());
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Route> NetUtils::routeTable()
{
    QList<Route> routes;

#if defined(Q_OS_WINDOWS)
    ULONG buffer_size = sizeof(MIB_IPFORWARDTABLE);

    std::vector<char> buffer(buffer_size);
    PMIB_IPFORWARDTABLE table = reinterpret_cast<PMIB_IPFORWARDTABLE>(buffer.data());

    DWORD error_code = GetIpForwardTable(table, &buffer_size, 0);
    if (error_code == ERROR_INSUFFICIENT_BUFFER)
    {
        buffer.resize(buffer_size);
        table = reinterpret_cast<PMIB_IPFORWARDTABLE>(buffer.data());
        error_code = GetIpForwardTable(table, &buffer_size, 0);
    }

    if (error_code != NO_ERROR)
        return routes;

    routes.reserve(table->dwNumEntries);
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_IPFORWARDROW& row = table->table[i];
        routes.emplaceBack(ipToString(row.dwForwardDest), ipToString(row.dwForwardMask),
                           ipToString(row.dwForwardNextHop), row.dwForwardMetric1);
    }
#elif defined(Q_OS_LINUX)
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
#elif defined(Q_OS_MACOS)
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
#endif

    return routes;
}

//--------------------------------------------------------------------------------------------------
// static
QString NetUtils::defaultGatewayAddress()
{
    QString best_gateway;
    quint32 best_metric = std::numeric_limits<quint32>::max();

    // The default route has destination 0.0.0.0; among those pick the one with the lowest metric.
    for (const Route& route : routeTable())
    {
        if (QHostAddress(route.destination) != QHostAddress(QHostAddress::AnyIPv4))
            continue;

        QHostAddress gateway(route.gateway);
        if (gateway.protocol() != QAbstractSocket::IPv4Protocol ||
            gateway == QHostAddress(QHostAddress::AnyIPv4))
        {
            continue;
        }

        if (route.metric < best_metric)
        {
            best_metric = route.metric;
            best_gateway = gateway.toString();
        }
    }

    return best_gateway;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isPrivateIpAddress(const QString& address)
{
    QHostAddress host_address(address);
    if (host_address.protocol() != QAbstractSocket::IPv4Protocol)
        return true;

    const quint32 ip = host_address.toIPv4Address();

    if ((ip & 0xFF000000) == 0x0A000000) // 10.0.0.0/8
        return true;
    if ((ip & 0xFFF00000) == 0xAC100000) // 172.16.0.0/12
        return true;
    if ((ip & 0xFFFF0000) == 0xC0A80000) // 192.168.0.0/16
        return true;
    if ((ip & 0xFFC00000) == 0x64400000) // 100.64.0.0/10 (CGNAT)
        return true;
    if ((ip & 0xFFFF0000) == 0xA9FE0000) // 169.254.0.0/16 (link-local)
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isValidIpAddress(const QString& ip_address)
{
    QHostAddress address(ip_address);
    return address.protocol() == QAbstractSocket::IPv4Protocol ||
           address.protocol() == QAbstractSocket::IPv6Protocol;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isValidHostName(const QString& host)
{
    if (host.isEmpty())
        return false;

    int length = host.length();
    if (length > kMaxHostNameLength)
        return false;

    size_t letter_count = 0;
    size_t digit_count = 0;

    for (int i = 0; i < length; ++i)
    {
        if (host[i].isDigit())
            ++digit_count;

        if (host[i].isLetter())
            ++letter_count;

        if (!isValidHostNameChar(host[i]))
            return false;
    }

    if (!letter_count && !digit_count)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isValidPort(quint32 port)
{
    return port != 0 && port <= 65535;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isValidPort(const QString& str)
{
    bool ok = false;
    quint16 value = str.toUShort(&ok);
    if (!ok)
        return false;

    return isValidPort(value);
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isValidSubnet(const QString& subnet)
{
    QPair<QHostAddress, int> parsed_subnet = QHostAddress::parseSubnet(subnet);
    if (parsed_subnet.second < 0)
        return false;

    return parsed_subnet.first.protocol() == QAbstractSocket::IPv4Protocol ||
           parsed_subnet.first.protocol() == QAbstractSocket::IPv6Protocol;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isAddressEqual(const QString& address1, const QString& address2)
{
    QHostAddress host_address1(address1);
    QHostAddress host_address2(address2);
    return host_address1 == host_address2;
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isAddressEqual(const std::string& address1, const std::string& address2)
{
    return isAddressEqual(QString::fromStdString(address1), QString::fromStdString(address2));
}

//--------------------------------------------------------------------------------------------------
// static
bool NetUtils::isAddressInWhiteList(const QStringList& white_list, const QString& address_string)
{
    if (white_list.isEmpty())
        return true;

    QHostAddress address(address_string);

    // Dual-stack listeners report IPv4 peers as IPv4-mapped IPv6 (::ffff:a.b.c.d). Normalize them
    // back to plain IPv4 so that both literal and subnet entries match.
    bool is_ipv4 = false;
    const quint32 ipv4 = address.toIPv4Address(&is_ipv4);
    if (is_ipv4)
        address = QHostAddress(ipv4);

    if (address.protocol() != QAbstractSocket::IPv4Protocol &&
        address.protocol() != QAbstractSocket::IPv6Protocol)
    {
        return false;
    }

    for (const auto& entry : white_list)
    {
        QHostAddress white_list_address(entry);
        if (white_list_address == address)
            return true;

        const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(entry);
        if (subnet.second >= 0 && address.isInSubnet(subnet))
            return true;
    }

    return false;
}
