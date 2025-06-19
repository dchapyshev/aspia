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

#ifndef BASE_DESKTOP_WIN_DISPLAY_CONFIGURATION_MONITOR_H
#define BASE_DESKTOP_WIN_DISPLAY_CONFIGURATION_MONITOR_H

#include <QRect>

namespace base {

// A passive monitor to detect the change of display configuration on a Windows system.
// TODO(zijiehe): Also check for pixel format changes.
class DisplayConfigurationMonitor
{
public:
    // Checks whether the change of display configuration has happened after last
    // isChanged() call. This function won't return true for the first time after
    // constructor or reset() call.
    bool isChanged();

    // Resets to the initial state.
    void reset();

private:
    QRect rect_;
    bool initialized_ = false;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DISPLAY_CONFIGURATION_MONITOR_H
