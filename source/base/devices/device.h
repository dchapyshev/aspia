//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__DEVICE_H
#define _ASPIA_BASE__DEVICES__DEVICE_H

#include "base/files/file_path.h"
#include "base/files/platform_file.h"

namespace aspia {

class Device
{
public:
    virtual ~Device();

    bool Open(const FilePath& device_path);
    void Close();
    bool SendIoControl(DWORD io_control_code,
                       LPVOID input_buffer,
                       DWORD input_buffer_size,
                       LPVOID output_buffer,
                       DWORD output_buffer_size,
                       LPDWORD bytes_returned);

private:
    ScopedPlatformFile device_;

    DISALLOW_COPY_AND_ASSIGN(Device);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__DEVICE_H
