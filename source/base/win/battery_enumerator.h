//
// SmartCafe Project
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

#ifndef BASE_WIN_BATTERY_ENUMERATOR_H
#define BASE_WIN_BATTERY_ENUMERATOR_H

#include <QObject>
#include <QString>

#include "base/win/device.h"
#include "base/win/scoped_device_info.h"

namespace base {

class BatteryEnumerator
{
    Q_GADGET

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
    Q_ENUM(State)

    QString deviceName() const;
    QString manufacturer() const;
    QString manufactureDate() const;
    QString uniqueId() const;
    QString serialNumber() const;
    QString temperature() const;
    quint32 designCapacity() const;
    QString type() const;
    quint32 fullChargedCapacity() const;
    quint32 depreciation() const;
    quint32 currentCapacity() const;
    quint32 voltage() const;
    quint32 state() const;

private:
    ScopedDeviceInfo device_info_;
    DWORD device_index_ = 0;

    mutable Device battery_;
    mutable DWORD battery_tag_ = 0;

    Q_DISABLE_COPY(BatteryEnumerator)
};

} // namespace base

#endif // BASE_WIN_BATTERY_ENUMERATOR_H
