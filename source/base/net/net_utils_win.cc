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

#include <qt_windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

#include <limits>
#include <vector>

namespace {

//--------------------------------------------------------------------------------------------------
QString ipToString(DWORD ip)
{
    char buffer[46 + 1];

    if (!inet_ntop(AF_INET, &ip, buffer, _countof(buffer)))
        return QString();

    return buffer;
}

//--------------------------------------------------------------------------------------------------
QString socketAddressString(const SOCKET_ADDRESS& address)
{
    if (!address.lpSockaddr || address.iSockaddrLength <= 0)
        return QString();

    char buffer[46 + 1] = { 0 };

    switch (address.lpSockaddr->sa_family)
    {
        case AF_INET:
        {
            auto* addr = reinterpret_cast<sockaddr_in*>(address.lpSockaddr);
            if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, _countof(buffer)))
                return QString();
        }
        break;

        case AF_INET6:
        {
            auto* addr = reinterpret_cast<sockaddr_in6*>(address.lpSockaddr);
            if (!inet_ntop(AF_INET6, &addr->sin6_addr, buffer, _countof(buffer)))
                return QString();
        }
        break;

        default:
            return QString();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
QString interfaceTypeName(DWORD type)
{
    switch (type)
    {
        case IF_TYPE_OTHER:
            return "Other";

        case IF_TYPE_ETHERNET_CSMACD:
            return "Ethernet";

        case IF_TYPE_ISO88025_TOKENRING:
            return "Token Ring";

        case IF_TYPE_PPP:
            return "PPP";

        case IF_TYPE_SOFTWARE_LOOPBACK:
            return "Software Lookback";

        case IF_TYPE_ATM:
            return "ATM";

        case IF_TYPE_IEEE80211:
            return "IEEE 802.11 Wireless";

        case IF_TYPE_TUNNEL:
            return "Tunnel type encapsulation";

        case IF_TYPE_IEEE1394:
            return "IEEE 1394 Firewire";

        default:
            return QString();
    }
}

//--------------------------------------------------------------------------------------------------
QString physicalAddressString(const BYTE* address, ULONG length)
{
    if (!length)
        return QString();

    QStringList parts;
    for (ULONG i = 0; i < length; ++i)
        parts << QString("%1").arg(address[i], 2, 16, QLatin1Char('0'));

    return parts.join(QLatin1Char('-')).toUpper();
}

//--------------------------------------------------------------------------------------------------
QString ipv4MaskString(ULONG prefix_length)
{
    in_addr mask;
    if (ConvertLengthToIpv4Mask(prefix_length, &mask.s_addr) != NO_ERROR)
        return QString();

    char buffer[46 + 1] = { 0 };
    if (!inet_ntop(AF_INET, &mask, buffer, _countof(buffer)))
        return QString();

    return buffer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Route> NetUtils::routeTable()
{
    QList<Route> routes;

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

    return routes;
}

//--------------------------------------------------------------------------------------------------
// static
QList<NetUtils::Adapter> NetUtils::adapters()
{
    QList<NetUtils::Adapter> result;

    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;

    ULONG buffer_size = sizeof(IP_ADAPTER_ADDRESSES);
    std::vector<quint8> buffer(buffer_size);

    auto head = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, head, &buffer_size);
    if (error_code == ERROR_BUFFER_OVERFLOW)
    {
        buffer.resize(buffer_size);
        head = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, head, &buffer_size);
    }

    if (error_code != ERROR_SUCCESS)
        return result;

    for (PIP_ADAPTER_ADDRESSES current = head; current != nullptr; current = current->Next)
    {
        NetUtils::Adapter adapter;

        if (current->Description)
            adapter.adapter_name = QString::fromWCharArray(current->Description);
        if (current->FriendlyName)
            adapter.connection_name = QString::fromWCharArray(current->FriendlyName);

        adapter.interface_type = interfaceTypeName(current->IfType);
        adapter.mtu = current->Mtu;
        adapter.speed = (current->TransmitLinkSpeed == std::numeric_limits<ULONG64>::max())
            ? 0 : current->TransmitLinkSpeed;
        adapter.mac = physicalAddressString(current->PhysicalAddress, current->PhysicalAddressLength);
        adapter.dhcp4_enabled = !!current->Dhcpv4Enabled;
        if (adapter.dhcp4_enabled)
            adapter.dhcp4_server = socketAddressString(current->Dhcpv4Server);

        for (auto* unicast = current->FirstUnicastAddress; unicast; unicast = unicast->Next)
        {
            NetUtils::Adapter::Address address;
            address.ip = socketAddressString(unicast->Address);
            address.mask = ipv4MaskString(unicast->OnLinkPrefixLength);
            adapter.addresses.append(std::move(address));
        }

        for (auto* gateway = current->FirstGatewayAddress; gateway; gateway = gateway->Next)
            adapter.gateways.append(socketAddressString(gateway->Address));

        for (auto* dns = current->FirstDnsServerAddress; dns; dns = dns->Next)
            adapter.dns_servers.append(socketAddressString(dns->Address));

        result.append(std::move(adapter));
    }

    return result;
}
