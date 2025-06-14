//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_POWER_INFO_H
#define BASE_WIN_POWER_INFO_H

#include <QtGlobal>
#include <qt_windows.h>

namespace base {

class PowerInfo
{
public:
    PowerInfo();
    ~PowerInfo();

    enum class PowerSource { UNKNOWN, DC_BATTERY, AC_LINE };
    enum class BatteryStatus { UNKNOWN, HIGH, LOW, CRITICAL, CHARGING, NO_BATTERY };

    PowerSource powerSource() const;
    BatteryStatus batteryStatus() const;
    quint32 batteryLifePercent() const;
    quint32 batteryFullLifeTime() const;
    quint32 batteryRemainingLifeTime() const;

private:
    SYSTEM_POWER_STATUS power_status_;
    bool initialized_ = false;

    Q_DISABLE_COPY(PowerInfo)
};

} // namespace base

#endif // BASE_WIN_POWER_INFO_H
