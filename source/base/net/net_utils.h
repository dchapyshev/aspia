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

    struct Adapter
    {
        struct Address
        {
            QString ip;
            QString mask;
        };

        QString adapter_name;
        QString connection_name;
        QString interface_type;
        quint32 mtu = 0;
        quint64 speed = 0;
        QString mac;
        bool dhcp4_enabled = false;
        QString dhcp4_server;
        QList<Address> addresses;
        QStringList gateways;
        QStringList dns_servers;
    };

    struct Connection
    {
        QString protocol;
        QString process_name;
        QString local_address;
        QString remote_address;
        quint16 local_port = 0;
        quint16 remote_port = 0;
        QString state;
    };

    struct Share
    {
        QString name;
        QString local_path;
        QString description;
        QString type;
        quint32 current_uses = 0;
        quint32 max_uses = 0;
    };

    struct OpenFile
    {
        quint32 id = 0;
        QString user_name;
        quint32 lock_count = 0;
        QString file_path;
    };

    // Returns the IP addresses of all active (up, non-loopback) interfaces that have a global address.
    static QStringList localIpList();

    // Returns the host's IPv4 routing table. Uses platform routing APIs, since Qt exposes no routing
    // information.
    static QList<Route> routeTable();

    // Returns the host's network adapters with their IPv4 configuration.
    static QList<Adapter> adapters();

    // Returns the host's active TCP and UDP connections (IPv4).
    static QList<Connection> connections();

    // Returns the SMB shares exported by the host.
    static QList<Share> networkShares();

    // Returns the files currently opened by remote clients over SMB.
    static QList<OpenFile> openFiles();

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
