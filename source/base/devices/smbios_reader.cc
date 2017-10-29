//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios_reader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/scoped_native_library.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

std::unique_ptr<SMBiosParser> ReadSMBiosFromFirmwareTable()
{
    typedef UINT(WINAPI *GetSystemFirmwareTableFunc)(DWORD, DWORD, PVOID, DWORD);

    ScopedNativeLibrary kernel32_library(L"kernel32.dll");

    GetSystemFirmwareTableFunc get_system_firmware_table_func =
        reinterpret_cast<GetSystemFirmwareTableFunc>(
            kernel32_library.GetFunctionPointer("GetSystemFirmwareTable"));

    if (!get_system_firmware_table_func)
    {
        DLOG(WARNING) << "GetSystemFirmwareTable() function not found";
        return nullptr;
    }

    UINT data_size = get_system_firmware_table_func('RSMB', 'PCAF', nullptr, 0);
    if (!data_size)
    {
        LOG(WARNING) << "GetSystemFirmwareTable() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(data_size);

    if (!get_system_firmware_table_func('RSMB', 'PCAF', data.get(), data_size))
    {
        LOG(WARNING) << "GetSystemFirmwareTable() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    return SMBiosParser::Create(std::move(data), data_size);
}

std::unique_ptr<SMBiosParser> ReadSMBiosFromWindowsRegistry()
{
    const WCHAR kSMBiosPath[] = L"SYSTEM\\CurrentControlSet\\services\\mssmbios\\Data";
    RegistryKey key;

    LONG status = key.Open(HKEY_LOCAL_MACHINE, kSMBiosPath, KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to open registry key: " << SystemErrorCodeToString(status);
        return nullptr;
    }

    DWORD type;
    DWORD data_size = 1;

    status = key.ReadValue(L"SMBiosData", nullptr, &data_size, &type);
    if (status != ERROR_MORE_DATA)
    {
        LOG(WARNING) << "Unexpected return value or data not available: "
                     << SystemErrorCodeToString(status);
        return nullptr;
    }

    if (type != REG_BINARY)
    {
        LOG(WARNING) << "Unexpected data type: " << type;
        return nullptr;
    }

    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(data_size);

    status = key.ReadValue(L"SMBiosData", data.get(), &data_size, &type);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to read SMBIOS data from registry: "
                     << SystemErrorCodeToString(status);
        return nullptr;
    }

    return SMBiosParser::Create(std::move(data), data_size);
}

std::unique_ptr<SMBiosParser> ReadSMBios()
{
    std::unique_ptr<SMBiosParser> parser = ReadSMBiosFromFirmwareTable();
    if (parser)
        return parser;

    return ReadSMBiosFromWindowsRegistry();
}

} // namespace aspia
