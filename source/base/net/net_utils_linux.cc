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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>

#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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

//--------------------------------------------------------------------------------------------------
QString readNetAttribute(const QString& interface, const char* attribute)
{
    QFile file(QString("/sys/class/net/%1/%2").arg(interface, QString::fromLatin1(attribute)));
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    return QString::fromLatin1(file.readLine()).trimmed();
}

//--------------------------------------------------------------------------------------------------
QString interfaceTypeName(const QString& interface)
{
    bool ok = false;
    const int type = readNetAttribute(interface, "type").toInt(&ok);
    if (!ok)
        return QString();

    switch (type)
    {
        case 772: // ARPHRD_LOOPBACK
            return "Software Lookback";

        case 1: // ARPHRD_ETHER. Wireless adapters report the same type; tell them apart by sysfs.
            if (QFile::exists(QString("/sys/class/net/%1/wireless").arg(interface)) ||
                QFile::exists(QString("/sys/class/net/%1/phy80211").arg(interface)))
            {
                return "IEEE 802.11 Wireless";
            }
            return "Ethernet";

        default:
            return "Other";
    }
}

//--------------------------------------------------------------------------------------------------
QString physicalAddressString(const QString& interface)
{
    QString mac = readNetAttribute(interface, "address").toUpper();
    mac.replace(QChar(':'), QChar('-'));
    return mac;
}

//--------------------------------------------------------------------------------------------------
quint64 linkSpeed(const QString& interface)
{
    bool ok = false;
    const qint64 speed_mbps = readNetAttribute(interface, "speed").toLongLong(&ok);
    if (!ok || speed_mbps <= 0)
        return 0;

    return static_cast<quint64>(speed_mbps) * 1000000ull;
}

//--------------------------------------------------------------------------------------------------
QString ipv4ToString(const sockaddr* address)
{
    if (!address || address->sa_family != AF_INET)
        return QString();

    char buffer[INET_ADDRSTRLEN] = { 0 };
    const auto* addr = reinterpret_cast<const sockaddr_in*>(address);

    if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)))
        return QString();

    return QString::fromLatin1(buffer);
}

//--------------------------------------------------------------------------------------------------
// Default gateways per interface, parsed from the kernel routing table.
QHash<QString, QStringList> defaultGateways()
{
    QHash<QString, QStringList> result;

    QFile file("/proc/net/route");
    if (!file.open(QIODevice::ReadOnly))
        return result;

    file.readLine(); // Skip the header row.

    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 3)
            continue;

        // The default route has a zero destination and a non-zero gateway.
        if (fields.at(1) != "00000000" || fields.at(2) == "00000000")
            continue;

        const QString gateway = hexToIpv4(fields.at(2));
        if (!gateway.isEmpty())
            result[QString::fromLatin1(fields.at(0))].append(gateway);
    }

    return result;
}

struct InterfaceInfo
{
    bool dhcp_enabled = false;
    QString dhcp_server;
    QStringList dns;
};

//--------------------------------------------------------------------------------------------------
// DHCPv4 state and DNS servers per interface, queried from NetworkManager. There is no backend-
// agnostic source, so this covers NetworkManager-managed systems and is empty elsewhere. Reading
// /etc/resolv.conf is avoided: with systemd-resolved it only yields the 127.0.0.53 stub address.
QHash<QString, InterfaceInfo> networkInfoByInterface()
{
    QHash<QString, InterfaceInfo> result;

    QProcess process;
    process.start("nmcli", QStringList()
        << "-t" << "-f" << "GENERAL.DEVICE,IP4.DNS,DHCP4.OPTION" << "device" << "show");
    if (!process.waitForStarted() || !process.waitForFinished())
        return result;

    QString current;

    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& raw : lines)
    {
        const QByteArray line = raw.trimmed();

        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QByteArray key = line.left(colon);
        const QByteArray value = line.mid(colon + 1);

        if (key == "GENERAL.DEVICE")
        {
            current = QString::fromUtf8(value);
            continue;
        }

        if (current.isEmpty())
            continue;

        if (key.startsWith("IP4.DNS"))
        {
            if (!value.isEmpty())
                result[current].dns.append(QString::fromUtf8(value));
        }
        else if (key.startsWith("DHCP4.OPTION"))
        {
            // The presence of any DHCPv4 option means the lease is active.
            InterfaceInfo& info = result[current];
            info.dhcp_enabled = true;

            // Each option reads like "dhcp_server_identifier = 192.168.1.1".
            const int equal = value.indexOf('=');
            if (equal > 0 && value.left(equal).trimmed() == "dhcp_server_identifier")
                info.dhcp_server = QString::fromUtf8(value.mid(equal + 1).trimmed());
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString connectionStateToString(int state)
{
    switch (state)
    {
        case 0x01: return "ESTABLISHED";
        case 0x02: return "SYN_SENT";
        case 0x03: return "SYN_RCVD";
        case 0x04: return "FIN_WAIT1";
        case 0x05: return "FIN_WAIT2";
        case 0x06: return "TIME_WAIT";
        case 0x07: return "CLOSED";
        case 0x08: return "CLOSE_WAIT";
        case 0x09: return "LAST_ACK";
        case 0x0A: return "LISTENING";
        case 0x0B: return "CLOSING";
        default:   return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString processName(const QString& pid)
{
    // Prefer the real executable name; /proc/<pid>/comm is truncated to 15 characters.
    char buffer[PATH_MAX];
    const QByteArray exe = QString("/proc/%1/exe").arg(pid).toLocal8Bit();
    const ssize_t length = readlink(exe.constData(), buffer, sizeof(buffer) - 1);
    if (length > 0)
    {
        QString target = QString::fromLocal8Bit(buffer, length);

        // A replaced binary keeps a " (deleted)" suffix on the link target.
        if (target.endsWith(" (deleted)"))
            target.chop(10);

        return QFileInfo(target).fileName();
    }

    // Fall back to comm for kernel threads and inaccessible processes.
    QFile comm(QString("/proc/%1/comm").arg(pid));
    if (comm.open(QIODevice::ReadOnly))
        return QString::fromLatin1(comm.readLine()).trimmed();

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Maps a socket inode to the owning process name by scanning the /proc/<pid>/fd symlinks. Only the
// caller's own sockets resolve unless running as root.
QHash<QString, QString> socketProcessNames()
{
    QHash<QString, QString> result;

    const QStringList pids = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& pid : pids)
    {
        bool is_pid = false;
        pid.toUInt(&is_pid);
        if (!is_pid)
            continue;

        const QString name = processName(pid);

        const QString fd_path = QString("/proc/%1/fd").arg(pid);
        DIR* fd_dir = opendir(fd_path.toLocal8Bit().constData());
        if (!fd_dir)
            continue;

        struct dirent* fd_entry;
        while ((fd_entry = readdir(fd_dir)) != nullptr)
        {
            char buffer[256];
            const QByteArray link =
                QString("%1/%2").arg(fd_path, QString::fromLatin1(fd_entry->d_name)).toLocal8Bit();

            const ssize_t length = readlink(link.constData(), buffer, sizeof(buffer) - 1);
            if (length <= 0)
                continue;

            // Socket links read as "socket:[12345]"; map the inode to the process name.
            const QByteArray target(buffer, length);
            if (!target.startsWith("socket:["))
                continue;

            result.insert(QString::fromLatin1(target.mid(8, target.size() - 9)), name);
        }

        closedir(fd_dir);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// Parses one /proc/net/{tcp,udp} table. |with_remote| reports the peer endpoint and the state; it is
// disabled for UDP, which is connectionless (matching the Windows path).
void parseProcNet(const QString& path, const QString& protocol, bool with_remote,
                  const QHash<QString, QString>& process_names, QList<NetUtils::Connection>* result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    file.readLine(); // Skip the header line.

    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        // Fields: sl[0] local_address[1] rem_address[2] st[3] ... inode[9]. Addresses are
        // "HEXIP:HEXPORT" with the IP in host-order hex of in_addr.s_addr.
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 10)
            continue;

        const QList<QByteArray> local = fields[1].split(':');
        if (local.size() != 2)
            continue;

        NetUtils::Connection connection;
        connection.protocol = protocol;
        connection.process_name = process_names.value(QString::fromLatin1(fields[9]));
        connection.local_address = hexToIpv4(local[0]);
        connection.local_port = local[1].toUShort(nullptr, 16);

        if (with_remote)
        {
            const QList<QByteArray> remote = fields[2].split(':');
            if (remote.size() == 2)
            {
                connection.remote_address = hexToIpv4(remote[0]);
                connection.remote_port = remote[1].toUShort(nullptr, 16);
            }

            connection.state = connectionStateToString(fields[3].toInt(nullptr, 16));
        }

        result->append(std::move(connection));
    }
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
    QList<NetUtils::Adapter> result;

    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0)
        return result;

    const QHash<QString, QStringList> gateways = defaultGateways();
    const QHash<QString, InterfaceInfo> net_info = networkInfoByInterface();
    QHash<QString, qsizetype> index_by_name;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_name)
            continue;

        const QString name = QString::fromLatin1(ifa->ifa_name);

        qsizetype index;
        auto it = index_by_name.constFind(name);
        if (it != index_by_name.constEnd())
        {
            index = it.value();
        }
        else
        {
            NetUtils::Adapter adapter;
            adapter.adapter_name = name;
            adapter.connection_name = name;
            adapter.interface_type = interfaceTypeName(name);
            adapter.mtu = readNetAttribute(name, "mtu").toUInt();
            adapter.speed = linkSpeed(name);
            adapter.mac = physicalAddressString(name);

            const InterfaceInfo info = net_info.value(name);
            adapter.dhcp4_enabled = info.dhcp_enabled;
            adapter.dhcp4_server = info.dhcp_server;
            adapter.dns_servers = info.dns;
            adapter.gateways = gateways.value(name);

            index = result.size();
            result.append(std::move(adapter));
            index_by_name.insert(name, index);
        }

        // Only IPv4 addresses are reported, matching the Windows path.
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            NetUtils::Adapter::Address address;
            address.ip = ipv4ToString(ifa->ifa_addr);
            address.mask = ipv4ToString(ifa->ifa_netmask);

            if (!address.ip.isEmpty())
                result[index].addresses.append(std::move(address));
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Connection> NetUtils::connections()
{
    QList<Connection> result;

    const QHash<QString, QString> process_names = socketProcessNames();

    parseProcNet("/proc/net/tcp", "TCP", true, process_names, &result);
    parseProcNet("/proc/net/udp", "UDP", false, process_names, &result);

    return result;
}
