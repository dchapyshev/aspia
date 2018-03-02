//
// PROJECT:         Aspia
// FILE:            base/devices/video_adapter_enumerator.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/video_adapter_enumerator.h"
#include "base/strings/unicode.h"

#include <devguid.h>

namespace aspia {

VideoAdapterEnumarator::VideoAdapterEnumarator()
    : DeviceEnumerator(&GUID_DEVCLASS_DISPLAY, DIGCF_PROFILE | DIGCF_PRESENT)
{
    // Nothing
}

std::string VideoAdapterEnumarator::GetAdapterString() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(L"HardwareInformation.AdapterString"));
}

std::string VideoAdapterEnumarator::GetBIOSString() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(L"HardwareInformation.BiosString"));
}

std::string VideoAdapterEnumarator::GetChipString() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(L"HardwareInformation.ChipType"));
}

std::string VideoAdapterEnumarator::GetDACType() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(L"HardwareInformation.DacType"));
}

uint64_t VideoAdapterEnumarator::GetMemorySize() const
{
    return GetDriverRegistryDW(L"HardwareInformation.MemorySize");
}

} // namespace aspia
