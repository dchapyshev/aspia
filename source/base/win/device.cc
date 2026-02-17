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

#include "base/win/device.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Device::~Device()
{
    close();
}

//--------------------------------------------------------------------------------------------------
bool Device::open(const QString& device_path,
                  DWORD desired_access,
                  DWORD share_mode)
{
    device_.reset(CreateFileW(qUtf16Printable(device_path),
                              desired_access,
                              share_mode,
                              nullptr,
                              OPEN_EXISTING,
                              0,
                              nullptr));
    return device_.isValid();
}

//--------------------------------------------------------------------------------------------------
bool Device::open(const QString& device_path)
{
    return open(device_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
}

//--------------------------------------------------------------------------------------------------
void Device::close()
{
    device_.reset();
}

//--------------------------------------------------------------------------------------------------
bool Device::ioControl(DWORD io_control_code,
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

} // namespace base
