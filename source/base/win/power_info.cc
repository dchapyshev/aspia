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

#include "base/win/power_info.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
PowerInfo::PowerInfo()
{
    memset(&power_status_, 0, sizeof(power_status_));
    if (!GetSystemPowerStatus(&power_status_))
    {
        PLOG(ERROR) << "GetSystemPowerStatus failed";
        return;
    }

    initialized_ = true;
}

//--------------------------------------------------------------------------------------------------
PowerInfo::~PowerInfo() = default;

//--------------------------------------------------------------------------------------------------
PowerInfo::PowerSource PowerInfo::powerSource() const
{
    if (!initialized_)
        return PowerSource::UNKNOWN;

    switch (power_status_.ACLineStatus)
    {
        case 0:
            return PowerSource::DC_BATTERY;

        case 1:
            return PowerSource::AC_LINE;

        default:
            return PowerSource::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
PowerInfo::BatteryStatus PowerInfo::batteryStatus() const
{
    if (!initialized_)
        return BatteryStatus::UNKNOWN;

    switch (power_status_.BatteryFlag)
    {
        case 1:
            return BatteryStatus::HIGH;

        case 2:
            return BatteryStatus::LOW;

        case 4:
            return BatteryStatus::CRITICAL;

        case 8:
            return BatteryStatus::CHARGING;

        case 128:
            return BatteryStatus::NO_BATTERY;

        default:
            return BatteryStatus::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
quint32 PowerInfo::batteryLifePercent() const
{
    if (!initialized_ || power_status_.BatteryFlag == 128)
        return 0;

    return power_status_.BatteryLifePercent;
}

//--------------------------------------------------------------------------------------------------
quint32 PowerInfo::batteryFullLifeTime() const
{
    if (!initialized_ || power_status_.BatteryFlag == 128)
        return 0;

    if (power_status_.BatteryFullLifeTime == 0xFFFFFFFF)
        return 0;

    return power_status_.BatteryFullLifeTime;
}

//--------------------------------------------------------------------------------------------------
quint32 PowerInfo::batteryRemainingLifeTime() const
{
    if (!initialized_ || power_status_.BatteryFlag == 128)
        return 0;

    if (power_status_.BatteryLifeTime == 0xFFFFFFFF)
        return 0;

    return power_status_.BatteryLifeTime;
}

} // namespace base
