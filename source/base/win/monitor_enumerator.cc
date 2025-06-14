//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/monitor_enumerator.h"

#include "base/logging.h"
#include "base/win/registry.h"

#include <devguid.h>

namespace base {

//--------------------------------------------------------------------------------------------------
MonitorEnumerator::MonitorEnumerator()
    : DeviceEnumerator(&GUID_DEVCLASS_MONITOR, DIGCF_PROFILE | DIGCF_PRESENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Edid> MonitorEnumerator::edid() const
{
    QString key_path = QString("SYSTEM\\CurrentControlSet\\Enum\\%1\\Device Parameters").arg(deviceID());

    RegistryKey key;
    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to open registry key:"
                   << SystemError(static_cast<DWORD>(status)).toString();
        return nullptr;
    }

    DWORD type;
    DWORD size = 128;
    std::unique_ptr<quint8[]> data = std::make_unique<quint8[]>(size);

    status = key.readValue("EDID", data.get(), &size, &type);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_MORE_DATA)
        {
            data = std::make_unique<quint8[]>(size);
            status = key.readValue("EDID", data.get(), &size, &type);
        }

        if (status != ERROR_SUCCESS)
        {
            LOG(ERROR) << "Unable to read EDID data from registry:"
                       << SystemError(static_cast<DWORD>(status)).toString();
            return nullptr;
        }
    }

    if (type != REG_BINARY)
    {
        LOG(ERROR) << "Unexpected data type:" << type;
        return nullptr;
    }

    return Edid::create(std::move(data), size);
}

} // namespace base
