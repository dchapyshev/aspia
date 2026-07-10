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

#include <QHash>
#include <QHostAddress>
#include <QProcess>
#include <QtEndian>

#include <ifaddrs.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/tcp_fsm.h>

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

//--------------------------------------------------------------------------------------------------
QString macToString(const quint8* bytes, int length)
{
    QStringList parts;
    for (int i = 0; i < length; ++i)
        parts.append(QString("%1").arg(bytes[i], 2, 16, QChar('0')));
    return parts.join(':').toUpper();
}

//--------------------------------------------------------------------------------------------------
QString interfaceTypeName(quint8 type)
{
    switch (type)
    {
        case IFT_ETHER:    return "Ethernet"; // Wi-Fi also reports as Ethernet on macOS.
        case IFT_LOOP:     return "Loopback";
        case IFT_BRIDGE:   return "Bridge";
        case IFT_L2VLAN:   return "VLAN";
        case IFT_PPP:      return "PPP";
        case IFT_CELLULAR: return "Cellular";
        case IFT_GIF:
        case IFT_STF:      return "Tunnel";
        default:           return "Other";
    }
}

//--------------------------------------------------------------------------------------------------
// Returns the DHCP lease option |option| for |interface_name|, or an empty string when the interface
// is not configured by DHCP. There is no non-service public API for per-interface DHCP data, so the
// query goes through ipconfig, which reads it straight from the lease.
QString ipconfigOption(const QString& interface_name, const char* option)
{
    QProcess process;
    process.start("/usr/sbin/ipconfig", QStringList() << "getoption" << interface_name << option);
    if (!process.waitForFinished(3000))
        return QString();

    return QString::fromLatin1(process.readAllStandardOutput()).trimmed();
}

//--------------------------------------------------------------------------------------------------
// Collects the default-route gateway of each interface from the routing socket dump, keyed by
// interface name.
QHash<QString, QStringList> defaultGatewaysByInterface()
{
    QHash<QString, QStringList> result;

    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };

    size_t needed = 0;
    if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) < 0 || needed == 0)
        return result;

    std::vector<char> buffer(needed);
    if (sysctl(mib, 6, buffer.data(), &needed, nullptr, 0) < 0)
        return result;

    char* end = buffer.data() + needed;
    for (char* next = buffer.data(); next < end;)
    {
        auto* message = reinterpret_cast<rt_msghdr*>(next);
        next += message->rtm_msglen;

        if (!(message->rtm_flags & RTF_GATEWAY))
            continue;

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

        // Only the default route (destination 0.0.0.0) contributes the interface's gateway.
        if (!table[RTAX_DST] || table[RTAX_DST]->sa_family != AF_INET)
            continue;
        if (reinterpret_cast<const sockaddr_in*>(table[RTAX_DST])->sin_addr.s_addr != 0)
            continue;

        const QString gateway = sockaddrToIpv4(table[RTAX_GATEWAY]);
        if (gateway.isEmpty())
            continue;

        char name[IF_NAMESIZE] = {};
        if (if_indextoname(message->rtm_index, name))
            result[QString::fromLatin1(name)].append(gateway);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString inAddrToString(const in_addr& address)
{
    return QHostAddress(qFromBigEndian<quint32>(address.s_addr)).toString();
}

//--------------------------------------------------------------------------------------------------
QString tcpStateName(int state)
{
    switch (state)
    {
        case TCPS_CLOSED:       return "CLOSED";
        case TCPS_LISTEN:       return "LISTEN";
        case TCPS_SYN_SENT:     return "SYN_SENT";
        case TCPS_SYN_RECEIVED: return "SYN_RECV";
        case TCPS_ESTABLISHED:  return "ESTABLISHED";
        case TCPS_CLOSE_WAIT:   return "CLOSE_WAIT";
        case TCPS_FIN_WAIT_1:   return "FIN_WAIT1";
        case TCPS_CLOSING:      return "CLOSING";
        case TCPS_LAST_ACK:     return "LAST_ACK";
        case TCPS_FIN_WAIT_2:   return "FIN_WAIT2";
        case TCPS_TIME_WAIT:    return "TIME_WAIT";
        default:                return QString();
    }
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
    QList<NetUtils::Adapter> result;

    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0)
        return result;

    const QHash<QString, QStringList> gateways = defaultGatewaysByInterface();
    QHash<QString, qsizetype> index_by_name;

    auto adapterFor = [&](const QString& name) -> NetUtils::Adapter&
    {
        auto it = index_by_name.constFind(name);
        if (it != index_by_name.constEnd())
            return result[it.value()];

        NetUtils::Adapter adapter;
        adapter.adapter_name = name;
        adapter.connection_name = name;
        adapter.gateways = gateways.value(name);

        const qsizetype index = result.size();
        result.append(std::move(adapter));
        index_by_name.insert(name, index);
        return result[index];
    };

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_name || !ifa->ifa_addr)
            continue;

        const QString name = QString::fromLatin1(ifa->ifa_name);

        // The link-layer entry carries the hardware address, MTU, link speed and interface type.
        if (ifa->ifa_addr->sa_family == AF_LINK)
        {
            NetUtils::Adapter& adapter = adapterFor(name);

            const auto* link = reinterpret_cast<const sockaddr_dl*>(ifa->ifa_addr);
            if (link->sdl_alen > 0)
                adapter.mac = macToString(reinterpret_cast<const quint8*>(LLADDR(link)),
                                          link->sdl_alen);

            if (ifa->ifa_data)
            {
                const auto* data = reinterpret_cast<const if_data*>(ifa->ifa_data);
                adapter.mtu = data->ifi_mtu;
                adapter.speed = data->ifi_baudrate;
                adapter.interface_type = interfaceTypeName(data->ifi_type);
            }
        }
        else if (ifa->ifa_addr->sa_family == AF_INET) // Only IPv4, matching the other platforms.
        {
            NetUtils::Adapter& adapter = adapterFor(name);

            NetUtils::Adapter::Address address;
            address.ip = sockaddrToIpv4(ifa->ifa_addr);
            address.mask = sockaddrToIpv4(ifa->ifa_netmask);

            if (!address.ip.isEmpty())
                adapter.addresses.append(std::move(address));
        }
    }

    freeifaddrs(ifaddr);

    // DHCP and DNS come from the lease, queried only for interfaces that actually carry an IPv4
    // address (the rest have no lease to report).
    for (NetUtils::Adapter& adapter : result)
    {
        if (adapter.addresses.isEmpty())
            continue;

        const QString server = ipconfigOption(adapter.adapter_name, "server_identifier");
        if (!server.isEmpty())
        {
            adapter.dhcp4_enabled = true;
            adapter.dhcp4_server = server;
        }

        const QString dns = ipconfigOption(adapter.adapter_name, "domain_name_server");
        if (!dns.isEmpty())
            adapter.dns_servers = dns.split(' ', Qt::SkipEmptyParts);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Connection> NetUtils::connections()
{
    QList<Connection> result;

    // Walk every process and its file descriptors; each socket descriptor yields a connection with
    // the owning process name. Reading other processes' descriptors requires root.
    int pids_size = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (pids_size <= 0)
        return result;

    std::vector<pid_t> pids(pids_size / sizeof(pid_t));
    pids_size = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), pids_size);
    if (pids_size <= 0)
        return result;

    const int pid_count = pids_size / sizeof(pid_t);
    for (int i = 0; i < pid_count; ++i)
    {
        const pid_t pid = pids[i];
        if (pid <= 0)
            continue;

        int fds_size = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
        if (fds_size <= 0)
            continue;

        std::vector<proc_fdinfo> fds(fds_size / sizeof(proc_fdinfo));
        fds_size = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds.data(), fds_size);
        if (fds_size <= 0)
            continue;

        char name_buffer[256] = {};
        QString process_name;
        if (proc_name(pid, name_buffer, sizeof(name_buffer)) > 0)
            process_name = QString::fromUtf8(name_buffer);

        const int fd_count = fds_size / sizeof(proc_fdinfo);
        for (int j = 0; j < fd_count; ++j)
        {
            if (fds[j].proc_fdtype != PROX_FDTYPE_SOCKET)
                continue;

            socket_fdinfo info = {};
            if (proc_pidfdinfo(pid, fds[j].proc_fd, PROC_PIDFDSOCKETINFO, &info, sizeof(info)) !=
                sizeof(info))
            {
                continue;
            }

            const socket_info& socket = info.psi;
            if (socket.soi_family != AF_INET) // IPv4 only, matching the other platforms.
                continue;

            const in_sockinfo* in = nullptr;
            Connection connection;

            if (socket.soi_kind == SOCKINFO_TCP)
            {
                in = &socket.soi_proto.pri_tcp.tcpsi_ini;
                connection.protocol = "TCP";
                connection.state = tcpStateName(socket.soi_proto.pri_tcp.tcpsi_state);
            }
            else if (socket.soi_kind == SOCKINFO_IN)
            {
                in = &socket.soi_proto.pri_in;
                connection.protocol = "UDP";
            }
            else
            {
                continue;
            }

            connection.process_name = process_name;
            connection.local_address = inAddrToString(in->insi_laddr.ina_46.i46a_addr4);
            connection.remote_address = inAddrToString(in->insi_faddr.ina_46.i46a_addr4);
            connection.local_port = ntohs(static_cast<quint16>(in->insi_lport));
            connection.remote_port = ntohs(static_cast<quint16>(in->insi_fport));

            result.append(std::move(connection));
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Share> NetUtils::networkShares()
{
    QList<Share> result;

    // "sharing -l" lists the configured share points. Each starts with a non-indented "name:"/"path:"
    // pair, followed by indented per-protocol blocks ("smb:", "afp:"). A share is exported over SMB
    // when its smb block reports "shared: 1".
    QProcess process;
    process.start("/usr/sbin/sharing", QStringList() << "-l");
    if (!process.waitForStarted() || !process.waitForFinished())
        return result;

    Share share;
    bool have_share = false;
    bool in_smb = false;
    bool smb_shared = false;

    auto flush = [&]()
    {
        if (have_share && smb_shared)
            result.append(share);
    };

    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& raw : lines)
    {
        const bool indented = raw.startsWith(' ') || raw.startsWith('\t');
        const QByteArray line = raw.trimmed();
        const int colon = line.indexOf(':');
        const QString value = colon >= 0 ? QString::fromUtf8(line.mid(colon + 1).trimmed()) : QString();

        if (!indented && line.startsWith("name:"))
        {
            flush();

            share = Share();
            share.name = value;
            share.type = "Disk";
            have_share = true;
            in_smb = false;
            smb_shared = false;
        }
        else if (!indented && line.startsWith("path:"))
        {
            share.local_path = value;
        }
        else if (line.startsWith("smb:"))
        {
            in_smb = true;
        }
        else if (line.startsWith("afp:"))
        {
            in_smb = false;
        }
        else if (in_smb && line.startsWith("shared:"))
        {
            smb_shared = (value == "1");
        }
    }

    flush();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::OpenFile> NetUtils::openFiles()
{
    // The macOS SMB server (Apple's own smbd, not Samba) exposes no server-side listing of files
    // opened by remote clients - there is no smbstatus equivalent and smbutil is client-only. So no
    // open-file information is available.
    return QList<NetUtils::OpenFile>();
}
