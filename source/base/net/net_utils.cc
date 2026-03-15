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

namespace base {

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
bool NetUtils::isValidPort(quint16 port)
{
    return port != 0;
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
};

} // namespace base
