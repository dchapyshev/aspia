//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_DEVICE_ENUMERATOR_H
#define BASE_WIN_DEVICE_ENUMERATOR_H

#include "base/win/scoped_device_info.h"

#include <string>

namespace base::win {

class DeviceEnumerator
{
public:
    DeviceEnumerator();
    virtual ~DeviceEnumerator();

    bool isAtEnd() const;
    void advance();

    std::string friendlyName() const;
    std::string description() const;
    std::string driverVersion() const;
    std::string driverDate() const;
    std::string driverVendor() const;
    std::string deviceID() const;

protected:
    DeviceEnumerator(const GUID* class_guid, DWORD flags);

    std::wstring driverRegistryString(const wchar_t* key_name) const;
    DWORD driverRegistryDW(const wchar_t* key_name) const;

private:
    std::wstring driverKeyPath() const;

    ScopedDeviceInfo device_info_;
    mutable SP_DEVINFO_DATA device_info_data_;
    DWORD device_index_ = 0;

    DISALLOW_COPY_AND_ASSIGN(DeviceEnumerator);
};

} // namespace base::win

#endif // BASE_WIN_DEVICE_ENUMERATOR_H
