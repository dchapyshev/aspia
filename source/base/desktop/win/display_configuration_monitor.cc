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

#include "base/desktop/win/display_configuration_monitor.h"

#include "base/desktop/win/screen_capture_utils.h"

namespace base {

//--------------------------------------------------------------------------------------------------
bool DisplayConfigurationMonitor::isChanged()
{
    QRect rect = ScreenCaptureUtils::fullScreenRect();
    if (!initialized_)
    {
        initialized_ = true;
        rect_ = rect;
        return false;
    }

    if (rect == rect_)
        return false;

    rect_ = rect;
    return true;
}

//--------------------------------------------------------------------------------------------------
void DisplayConfigurationMonitor::reset()
{
    initialized_ = false;
}

} // namespace base
