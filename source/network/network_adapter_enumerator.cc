//
// PROJECT:         Aspia
// FILE:            network/network_adapter_enumerator.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_adapter_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"

#include <setupapi.h>
#include <devguid.h>

namespace aspia {

//
// NetworkAdapterEnumerator
//

NetworkAdapterEnumerator::NetworkAdapterEnumerator()
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

bool NetworkAdapterEnumerator::IsAtEnd() const
{
    return adapter_ == nullptr;
}

void NetworkAdapterEnumerator::Advance()
{
    adapter_ = adapter_->Next;
}

static std::wstring GetAdapterRegistryPath(const char* adapter_name)
{
    static constexpr wchar_t kFormat[] =
        L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%S\\Connection";

    return StringPrintf(kFormat, adapter_name);
}

std::string NetworkAdapterEnumerator::GetAdapterName() const
{
    HDEVINFO device_info =
        SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);

    if (device_info == INVALID_HANDLE_VALUE)
        return std::string();

    std::wstring key_path = GetAdapterRegistryPath(adapter_->AdapterName);

    RegistryKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (!key.IsValid())
    {
        SetupDiDestroyDeviceInfoList(device_info);
        return std::string();
    }

    std::wstring adapter_id;

    if (key.ReadValue(L"PnpInstanceID", &adapter_id) != ERROR_SUCCESS)
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
                return UTF8fromUNICODE(device_name);
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
                return UTF8fromUNICODE(device_name);
            }

            break;
        }
    }

    SetupDiDestroyDeviceInfoList(device_info);
    return std::string();
}

std::string NetworkAdapterEnumerator::GetConnectionName() const
{
    std::wstring key_path = GetAdapterRegistryPath(adapter_->AdapterName);

    RegistryKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (!key.IsValid())
        return std::string();

    std::wstring name;

    if (key.ReadValue(L"Name", &name) != ERROR_SUCCESS)
        return std::string();

    return UTF8fromUNICODE(name);
}

std::string NetworkAdapterEnumerator::GetInterfaceType() const
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

uint32_t NetworkAdapterEnumerator::GetMtu() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return 0;

    return adapter_if_entry.dwMtu;
}

uint32_t NetworkAdapterEnumerator::GetSpeed() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return 0;

    return adapter_if_entry.dwSpeed;
}

std::string NetworkAdapterEnumerator::GetMacAddress() const
{
    MIB_IFROW adapter_if_entry;

    memset(&adapter_if_entry, 0, sizeof(adapter_if_entry));
    adapter_if_entry.dwIndex = adapter_->Index;

    if (GetIfEntry(&adapter_if_entry) != NO_ERROR)
        return std::string();

    return StringPrintf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
                        adapter_if_entry.bPhysAddr[0],
                        adapter_if_entry.bPhysAddr[1],
                        adapter_if_entry.bPhysAddr[2],
                        adapter_if_entry.bPhysAddr[3],
                        adapter_if_entry.bPhysAddr[4],
                        adapter_if_entry.bPhysAddr[5],
                        adapter_if_entry.bPhysAddr[6]);
}

bool NetworkAdapterEnumerator::IsWinsEnabled() const
{
    return !!adapter_->HaveWins;
}

std::string NetworkAdapterEnumerator::GetPrimaryWinsServer() const
{
    if (!IsWinsEnabled())
        return std::string();

    return adapter_->PrimaryWinsServer.IpAddress.String;
}

std::string NetworkAdapterEnumerator::GetSecondaryWinsServer() const
{
    if (!IsWinsEnabled())
        return std::string();

    return adapter_->SecondaryWinsServer.IpAddress.String;
}

bool NetworkAdapterEnumerator::IsDhcpEnabled() const
{
    return !!adapter_->DhcpEnabled;
}

//
// IpAddressEnumerator
//

NetworkAdapterEnumerator::IpAddressEnumerator::IpAddressEnumerator(
    const NetworkAdapterEnumerator& adapter)
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

void NetworkAdapterEnumerator::IpAddressEnumerator::Advance()
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

std::string NetworkAdapterEnumerator::IpAddressEnumerator::GetIpAddress() const
{
    return address_->IpAddress.String;
}

std::string NetworkAdapterEnumerator::IpAddressEnumerator::GetIpMask() const
{
    return address_->IpMask.String;
}

//
// GatewayEnumerator
//

NetworkAdapterEnumerator::GatewayEnumerator::GatewayEnumerator(
    const NetworkAdapterEnumerator& adapter)
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

void NetworkAdapterEnumerator::GatewayEnumerator::Advance()
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

std::string NetworkAdapterEnumerator::GatewayEnumerator::GetAddress() const
{
    return address_->IpAddress.String;
}

//
// DhcpEnumerator
//

NetworkAdapterEnumerator::DhcpEnumerator::DhcpEnumerator(
    const NetworkAdapterEnumerator& adapter)
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

void NetworkAdapterEnumerator::DhcpEnumerator::Advance()
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

std::string NetworkAdapterEnumerator::DhcpEnumerator::GetAddress() const
{
    return address_->IpAddress.String;
}

//
// DnsEnumerator
//

NetworkAdapterEnumerator::DnsEnumerator::DnsEnumerator(const NetworkAdapterEnumerator& adapter)
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

void NetworkAdapterEnumerator::DnsEnumerator::Advance()
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

std::string NetworkAdapterEnumerator::DnsEnumerator::GetAddress() const
{
    return address_->IpAddress.String;
}

} // namespace aspia
