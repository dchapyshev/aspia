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

#include "base/win/monitor_enumerator.h"

#include <devguid.h>

#include "base/logging.h"
#include "base/win/registry.h"

namespace base {

//--------------------------------------------------------------------------------------------------
MonitorEnumerator::MonitorEnumerator()
    : DeviceEnumerator(&GUID_DEVCLASS_MONITOR, DIGCF_PROFILE | DIGCF_PRESENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Edid MonitorEnumerator::edid() const
{
    QString key_path = QString("SYSTEM\\CurrentControlSet\\Enum\\%1\\Device Parameters").arg(deviceID());

    RegistryKey key;
    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to open registry key:"
                   << SystemError(static_cast<DWORD>(status)).toString();
        return Edid();
    }

    QByteArray buffer;
    status = key.readValueBIN("EDID", &buffer);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to read EDID data from registry:"
                   << SystemError(static_cast<DWORD>(status)).toString();
        return Edid();
    }

    return Edid(buffer);
}

} // namespace base
