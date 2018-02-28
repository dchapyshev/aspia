//
// PROJECT:         Aspia
// FILE:            base/devices/smbios_reader.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__SMBIOS_READER_H
#define _ASPIA_BASE__DEVICES__SMBIOS_READER_H

#include "base/devices/smbios.h"

namespace aspia {

std::unique_ptr<SMBios> ReadSMBios();

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_READER_H
