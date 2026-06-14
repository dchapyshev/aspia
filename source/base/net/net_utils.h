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

#ifndef BASE_NET_NET_UTILS_H
#define BASE_NET_NET_UTILS_H

#include <QList>
#include <QStringList>

class NetUtils
{
public:
    struct Route
    {
        QString destination;
        QString mask;
        QString gateway;
        quint32 metric = 0;
    };

    // Returns the IP addresses of all active (up, non-loopback) interfaces that have a global address.
    static QStringList localIpList();

    // Returns the host's IPv4 routing table. Uses platform routing APIs, since Qt exposes no routing
    // information.
    static QList<Route> routeTable();

    // Returns the IPv4 address of the host's default gateway, or an empty string if it cannot be
    // determined.
    static QString defaultGatewayAddress();

    // Returns true if |address| is a private/non-routable IPv4 address (RFC1918, CGNAT 100.64/10,
    // link-local).
    static bool isPrivateIpAddress(const QString& address);

    // Returns true if |ip_address| is a valid IPv4 or IPv6 literal.
    static bool isValidIpAddress(const QString& ip_address);

    // Returns true if |host| is a syntactically valid host name.
    static bool isValidHostName(const QString& host);

    // Returns true if the value is a valid port number (1-65535).
    static bool isValidPort(quint32 port);
    static bool isValidPort(const QString& str);

    // Returns true if |subnet| is a valid IPv4/IPv6 subnet in CIDR notation.
    static bool isValidSubnet(const QString& subnet);

    // Returns true if both arguments parse to the same IP address.
    static bool isAddressEqual(const QString& address1, const QString& address2);
    static bool isAddressEqual(const std::string& address1, const std::string& address2);

    // Returns true if |address| matches an entry in |white_list| (a literal IP or a CIDR subnet).
    // An empty list allows any address. IPv4-mapped IPv6 peers are normalized to plain IPv4 so that
    // they match IPv4 entries.
    static bool isAddressInWhiteList(const QStringList& white_list, const QString& address);

private:
    Q_DISABLE_COPY_MOVE(NetUtils)
};

#endif // BASE_NET_NET_UTILS_H
