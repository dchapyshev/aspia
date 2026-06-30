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

#include <limits>

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
