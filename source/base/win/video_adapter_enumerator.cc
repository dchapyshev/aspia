//
// SmartCafe Project
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

#include "base/win/video_adapter_enumerator.h"

#include <devguid.h>

namespace base {

//--------------------------------------------------------------------------------------------------
VideoAdapterEnumarator::VideoAdapterEnumarator()
    : DeviceEnumerator(&GUID_DEVCLASS_DISPLAY, DIGCF_PROFILE | DIGCF_PRESENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString VideoAdapterEnumarator::adapterString() const
{
    return driverRegistryString("HardwareInformation.AdapterString");
}

//--------------------------------------------------------------------------------------------------
QString VideoAdapterEnumarator::biosString() const
{
    return driverRegistryString("HardwareInformation.BiosString");
}

//--------------------------------------------------------------------------------------------------
QString VideoAdapterEnumarator::chipString() const
{
    return driverRegistryString("HardwareInformation.ChipType");
}

//--------------------------------------------------------------------------------------------------
QString VideoAdapterEnumarator::dacType() const
{
    return driverRegistryString("HardwareInformation.DacType");
}

//--------------------------------------------------------------------------------------------------
quint64 VideoAdapterEnumarator::memorySize() const
{
    return driverRegistryDW("HardwareInformation.MemorySize");
}

} // namespace base
