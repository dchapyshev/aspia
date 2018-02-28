//
// PROJECT:         Aspia
// FILE:            base/devices/device.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device.h"
#include "base/logging.h"

namespace aspia {

Device::~Device()
{
    Close();
}

bool Device::Open(const std::experimental::filesystem::path& device_path,
                  DWORD desired_access,
                  DWORD share_mode)
{
    device_.Reset(CreateFileW(device_path.c_str(),
                              desired_access,
                              share_mode,
                              nullptr,
                              OPEN_EXISTING,
                              0,
                              nullptr));
    return device_.IsValid();
}

bool Device::Open(const std::experimental::filesystem::path& device_path)
{
    return Open(device_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
}

void Device::Close()
{
    device_.Reset();
}

bool Device::IoControl(DWORD io_control_code,
                       LPVOID input_buffer,
                       DWORD input_buffer_size,
                       LPVOID output_buffer,
                       DWORD output_buffer_size,
                       LPDWORD bytes_returned)
{
    return !!DeviceIoControl(device_, io_control_code,
                             input_buffer, input_buffer_size,
                             output_buffer, output_buffer_size,
                             bytes_returned, nullptr);
}

} // namespace aspia
