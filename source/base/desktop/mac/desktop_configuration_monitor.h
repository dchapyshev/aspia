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

#ifndef BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_MONITOR_H
#define BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_MONITOR_H

#include "base/desktop/mac/desktop_configuration.h"

#include <memory>
#include <mutex>
#include <set>

#include <ApplicationServices/ApplicationServices.h>

namespace base {

// The class provides functions to synchronize capturing and display
// reconfiguring across threads, and the up-to-date MacDesktopConfiguration.
class DesktopConfigurationMonitor final
{
public:
    DesktopConfigurationMonitor();
    ~DesktopConfigurationMonitor();

    // Returns the current desktop configuration.
    MacDesktopConfiguration desktopConfiguration();

private:
    static void displaysReconfiguredCallback(CGDirectDisplayID display,
                                             CGDisplayChangeSummaryFlags flags,
                                             void* user_parameter);
    void displaysReconfigured(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags);

    std::mutex desktop_configuration_lock_;
    MacDesktopConfiguration desktop_configuration_;
    std::set<CGDirectDisplayID> reconfiguring_displays_;

    Q_DISABLE_COPY(DesktopConfigurationMonitor)
};

} // namespace base

#endif // BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_MONITOR_H
