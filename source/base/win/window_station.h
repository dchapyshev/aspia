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

#ifndef BASE_WIN_WINDOW_STATION_H
#define BASE_WIN_WINDOW_STATION_H

#include <QStringList>
#include <qt_windows.h>

#include "base/macros_magic.h"

namespace base {

class WindowStation
{
public:
    WindowStation();
    WindowStation(WindowStation&& other) noexcept;
    ~WindowStation();

    static WindowStation open(const QString& name);
    static WindowStation forCurrentProcess();
    static QStringList windowStationList();

    bool isValid() const;
    HWINSTA get() const;
    bool setProcessWindowStation();
    QString name();
    void close();

    WindowStation& operator=(WindowStation&& other) noexcept;

private:
    explicit WindowStation(HWINSTA winsta, bool own);

    static BOOL CALLBACK enumWindowStationProc(LPWSTR window_station, LPARAM lparam);

    HWINSTA winsta_ = nullptr;
    bool own_ = false;

    DISALLOW_COPY_AND_ASSIGN(WindowStation);
};

} // namespace base

#endif // BASE_WIN_WINDOW_STATION_H
