//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device.h"
#include "base/logging.h"

namespace aspia {

Device::~Device()
{
    Close();
}

bool Device::Open(const FilePath& device_path)
{
    device_.Reset(CreateFileW(device_path.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr,
                              OPEN_EXISTING,
                              0,
                              nullptr));
    if (device_.IsValid())
    {
        LOG(WARNING) << "Unable to open device: " << device_path
                     << ". Error code: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

void Device::Close()
{
    device_.Reset();
}

bool Device::SendIoControl(DWORD io_control_code,
                           LPVOID input_buffer,
                           DWORD input_buffer_size,
                           LPVOID output_buffer,
                           DWORD output_buffer_size,
                           LPDWORD bytes_returned)
{
    if (!DeviceIoControl(device_, io_control_code,
                         input_buffer, input_buffer_size,
                         output_buffer, output_buffer_size,
                         bytes_returned, nullptr))
    {
        LOG(WARNING) << "DeviceIoControl() failed: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

} // namespace aspia
