//
// PROJECT:         Aspia
// FILE:            base/devices/smbios_reader.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/logging.h"

namespace aspia {

std::unique_ptr<SMBios> ReadSMBios()
{
    UINT data_size = GetSystemFirmwareTable('RSMB', 'PCAF', nullptr, 0);
    if (!data_size)
    {
        DPLOG(LS_WARNING) << "GetSystemFirmwareTable failed";
        return nullptr;
    }

    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(data_size);

    if (!GetSystemFirmwareTable('RSMB', 'PCAF', data.get(), data_size))
    {
        DPLOG(LS_WARNING) << "GetSystemFirmwareTable failed";
        return nullptr;
    }

    return SMBios::Create(std::move(data), data_size);
}

} // namespace aspia
