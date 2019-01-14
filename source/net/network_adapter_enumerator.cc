//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "net/network_adapter_enumerator.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>
#include <setupapi.h>
#include <devguid.h>

#include "base/win/registry.h"
#include "base/string_printf.h"
#include "base/unicode.h"

namespace net {

//
// AdapterEnumerator
//

AdapterEnumerator::AdapterEnumerator()
{
    ULONG buffer_size = sizeof(IP_ADAPTER_INFO);

    adapters_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
    adapter_ = reinterpret_cast<PIP_ADAPTER_INFO>(adapters_buffer_.get());

    ULONG error_code = GetAdaptersInfo(adapter_, &buffer_size);
    if (error_code != ERROR_SUCCESS)
    {
        if (error_code != ERROR_BUFFER_OVERFLOW)
        {
            adapters_buffer_.reset();
            adapter_ = nullptr;
        }
        else
        {
            adapters_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
            adapter_ = reinterpret_cast<PIP_ADAPTER_INFO>(adapters_buffer_.get());

            error_code = GetAdaptersInfo(adapter_, &buffer_size);
            if (error_code != ERROR_SUCCESS)
            {
                adapters_buffer_.reset();
                adapter_ = nullptr;
            }
        }
    }
}

AdapterEnumerator::~AdapterEnumerator() = default;

bool AdapterEnumerator::isAtEnd() const
{
    return adapter_ == nullptr;
}

void AdapterEnumerator::advance()
{
    adapter_ = adapter_->Next;
}

static std::wstring GetAdapterRegistryPath(const char* adapter_name)
{
    static constexpr wchar_t kFormat[] =
        L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%S\\Connection";

    return base::stringPrintf(kFormat, adapter_name);
}

std::string AdapterEnumerator::adapterName() const
{
    HDEVINFO device_info =
        SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);

    if (device_info == INVALID_HANDLE_VALUE)
        return std::string();

    std::wstring key_path = GetAdapterRegistryPath(adapter_->AdapterName);

    base::win::RegistryKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (!key.isValid())
    {
        SetupDiDestroyDeviceInfoList(device_info);
        return std::string();
    }

    std::wstring adapter_id;

    if (key.readValue(L"PnpInstanceID", &adapter_id) != ERROR_SUCCESS)
    {
        SetupDiDestroyDeviceInfoList(device_info);
        return std::string();
    }

    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(device_info_data);

    DWORD device_index = 0;

    while (SetupDiEnumDeviceInfo(device_info, device_index, &device_info_data))
    {
        ++device_index;

        wchar_t device_id[MAX_PATH] = { 0 };

        if (!SetupDiGetDeviceInstanceIdW(device_info,
                                         &device_info_data,
                                         device_id,
                                         ARRAYSIZE(device_id),
                                         nullptr))
        {
            continue;
        }

        if (_wcsicmp(adapter_id.c_str(), device_id) == 0)
        {
            wchar_t device_name[MAX_PATH] = { 0 };

            if (SetupDiGetDeviceRegistryPropertyW(device_info,
                                                  &device_info_data,
                                                  SPDRP_FRIENDLYNAME,
                                                  nullptr,
                                                  reinterpret_cast<PBYTE>(device_name),
                                                  ARRAYSIZE(device_name),
                                                  nullptr))
            {
                SetupDiDestroyDeviceInfoList(device_info);
                return base::UTF8fromUTF16(device_name);
            }

            if (SetupDiGetDeviceRegistryPropertyW(device_info,
                                                  &device_info_data,
                                                  SPDRP_DEVICEDESC,
                                                  nullptr,
                                                  reinterpret_cast<PBYTE>(device_name),
                                                  ARRAYSIZE(device_name),
                                                  nullptr))
            {
                SetupDiDestroyDeviceInfoList(device_info);
                return base::UTF8fromUTF16(device_name);
            }

            break;
        }
    }

    SetupDiDestroyDeviceInfoList(device_info);
    return std::string();
}

std::string AdapterEnumerator::connectionName() const
{
    std::wstring key_path = GetAdapterRegistryPath(adapter_->AdapterName);

    base::win::RegistryKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (!key.isValid())
        return std::string();

    std::wstring name;

    if (key.readValue(L"Name", &name) != ERROR_SUCCESS)
        return std::string();

    return base::UTF8fromUTF16(name);
}

std::string AdapterEnumerator::interfaceType() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return std::string();

    switch (adapter_if_entry.dwType)
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
            return std::string();
    }
}

uint32_t AdapterEnumerator::mtu() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return 0;

    return adapter_if_entry.dwMtu;
}

uint32_t AdapterEnumerator::speed() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return 0;

    return adapter_if_entry.dwSpeed;
}

std::string AdapterEnumerator::macAddress() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return std::string();

    return base::stringPrintf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
                              adapter_if_entry.bPhysAddr[0],
                              adapter_if_entry.bPhysAddr[1],
                              adapter_if_entry.bPhysAddr[2],
                              adapter_if_entry.bPhysAddr[3],
                              adapter_if_entry.bPhysAddr[4],
                              adapter_if_entry.bPhysAddr[5],
                              adapter_if_entry.bPhysAddr[6]);
}

bool AdapterEnumerator::isWinsEnabled() const
{
    return !!adapter_->HaveWins;
}

std::string AdapterEnumerator::primaryWinsServer() const
{
    if (!isWinsEnabled())
        return std::string();

    return adapter_->PrimaryWinsServer.IpAddress.String;
}

std::string AdapterEnumerator::secondaryWinsServer() const
{
    if (!isWinsEnabled())
        return std::string();

    return adapter_->SecondaryWinsServer.IpAddress.String;
}

bool AdapterEnumerator::isDhcpEnabled() const
{
    return !!adapter_->DhcpEnabled;
}

//
// IpAddressEnumerator
//

AdapterEnumerator::IpAddressEnumerator::IpAddressEnumerator(const AdapterEnumerator& adapter)
    : address_(&adapter.adapter_->IpAddressList)
{
    while (true)
    {
        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }

        address_ = address_->Next;
    }
}

bool AdapterEnumerator::IpAddressEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::IpAddressEnumerator::advance()
{
    while (true)
    {
        address_ = address_->Next;

        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }
    }
}

std::string AdapterEnumerator::IpAddressEnumerator::address() const
{
    return address_->IpAddress.String;
}

std::string AdapterEnumerator::IpAddressEnumerator::mask() const
{
    return address_->IpMask.String;
}

//
// GatewayEnumerator
//

AdapterEnumerator::GatewayEnumerator::GatewayEnumerator(const AdapterEnumerator& adapter)
    : address_(&adapter.adapter_->GatewayList)
{
    while (true)
    {
        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }

        address_ = address_->Next;
    }
}

bool AdapterEnumerator::GatewayEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::GatewayEnumerator::advance()
{
    while (true)
    {
        address_ = address_->Next;

        if (!address_)
            break;

        if (strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
            break;
    }
}

std::string AdapterEnumerator::GatewayEnumerator::address() const
{
    return address_->IpAddress.String;
}

//
// DhcpEnumerator
//

AdapterEnumerator::DhcpEnumerator::DhcpEnumerator(const AdapterEnumerator& adapter)
    : address_(&adapter.adapter_->DhcpServer)
{
    while (true)
    {
        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }

        address_ = address_->Next;
    }
}

bool AdapterEnumerator::DhcpEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::DhcpEnumerator::advance()
{
    while (true)
    {
        address_ = address_->Next;

        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }
    }
}

std::string AdapterEnumerator::DhcpEnumerator::address() const
{
    return address_->IpAddress.String;
}

//
// DnsEnumerator
//

AdapterEnumerator::DnsEnumerator::DnsEnumerator(const AdapterEnumerator& adapter)
{
    ULONG buffer_size = sizeof(IP_PER_ADAPTER_INFO);

    info_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
    PIP_PER_ADAPTER_INFO info = reinterpret_cast<PIP_PER_ADAPTER_INFO>(info_buffer_.get());

    ULONG error_code = GetPerAdapterInfo(adapter.adapter_->Index, info, &buffer_size);
    if (error_code != ERROR_SUCCESS)
    {
        if (error_code != ERROR_BUFFER_OVERFLOW)
        {
            info_buffer_.reset();
        }
        else
        {
            info_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
            info = reinterpret_cast<PIP_PER_ADAPTER_INFO>(info_buffer_.get());

            error_code = GetPerAdapterInfo(adapter.adapter_->Index, info, &buffer_size);
            if (error_code != ERROR_SUCCESS)
            {
                info_buffer_.reset();
            }
        }
    }

    if (info_buffer_)
    {
        address_ = &info->DnsServerList;

        while (true)
        {
            if (!address_)
                break;

            if (address_->IpAddress.String[0] != 0 &&
                strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
            {
                break;
            }

            address_ = address_->Next;
        }
    }
}

bool AdapterEnumerator::DnsEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::DnsEnumerator::advance()
{
    while (true)
    {
        address_ = address_->Next;

        if (!address_)
            break;

        if (address_->IpAddress.String[0] != 0 &&
            strcmp(address_->IpAddress.String, "0.0.0.0") != 0)
        {
            break;
        }
    }
}

std::string AdapterEnumerator::DnsEnumerator::address() const
{
    return address_->IpAddress.String;
}

} // namespace net
