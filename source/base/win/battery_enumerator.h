//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__WIN__BATTERY_ENUMERATOR_H
#define BASE__WIN__BATTERY_ENUMERATOR_H

#include "base/win/device.h"
#include "base/win/scoped_device_info.h"

#include <string>

namespace base::win {

class BatteryEnumerator
{
public:
    BatteryEnumerator();
    ~BatteryEnumerator();

    bool isAtEnd() const;
    void advance();

    enum State
    {
        UNKNOWN      = 0,
        CHARGING     = 1,
        CRITICAL     = 2,
        DISCHARGING  = 4,
        POWER_ONLINE = 8
    };

    std::string deviceName() const;
    std::string manufacturer() const;
    std::string manufactureDate() const;
    std::string uniqueId() const;
    std::string serialNumber() const;
    std::string temperature() const;
    uint32_t designCapacity() const;
    std::string type() const;
    uint32_t fullChargedCapacity() const;
    uint32_t depreciation() const;
    uint32_t currentCapacity() const;
    uint32_t voltage() const;
    uint32_t state() const;

private:
    ScopedDeviceInfo device_info_;
    DWORD device_index_ = 0;

    mutable Device battery_;
    mutable DWORD battery_tag_ = 0;

    DISALLOW_COPY_AND_ASSIGN(BatteryEnumerator);
};

} // namespace base::win

#endif // BASE__WIN__BATTERY_ENUMERATOR_H
