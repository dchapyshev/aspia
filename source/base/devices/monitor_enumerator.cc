//
// PROJECT:         Aspia
// FILE:            base/devices/monitor_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/monitor_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"

#include <devguid.h>

namespace aspia {

MonitorEnumerator::MonitorEnumerator()
    : DeviceEnumerator(&GUID_DEVCLASS_MONITOR, DIGCF_PROFILE | DIGCF_PRESENT)
{
    // Nothing
}

std::unique_ptr<Edid> MonitorEnumerator::GetEDID() const
{
    std::wstring key_path =
        StringPrintf(L"SYSTEM\\CurrentControlSet\\Enum\\%s\\Device Parameters",
                     UNICODEfromUTF8(GetDeviceID()).c_str());

    RegistryKey key;

    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        DLOG(WARNING) << "Unable to open registry key: " << SystemErrorCodeToString(status);
        return nullptr;
    }

    DWORD type;
    DWORD size = 128;
    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(size);

    status = key.ReadValue(L"EDID", data.get(), &size, &type);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_MORE_DATA)
        {
            data = std::make_unique<uint8_t[]>(size);
            status = key.ReadValue(L"EDID", data.get(), &size, &type);
        }

        if (status != ERROR_SUCCESS)
        {
            DLOG(WARNING) << "Unable to read EDID data from registry: "
                          << SystemErrorCodeToString(status);
            return nullptr;
        }
    }

    if (type != REG_BINARY)
    {
        DLOG(WARNING) << "Unexpected data type: " << type;
        return nullptr;
    }

    return Edid::Create(std::move(data), size);
}

} // namespace aspia
