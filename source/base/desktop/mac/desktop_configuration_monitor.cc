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

#include "base/desktop/mac/desktop_configuration_monitor.h"

#include "base/logging.h"
#include "base/desktop/mac/desktop_configuration.h"

namespace base {

//--------------------------------------------------------------------------------------------------
DesktopConfigurationMonitor::DesktopConfigurationMonitor()
{
    CGError err = CGDisplayRegisterReconfigurationCallback(
        DesktopConfigurationMonitor::displaysReconfiguredCallback, this);
    if (err != kCGErrorSuccess)
        LOG(LS_ERROR) << "CGDisplayRegisterReconfigurationCallback " << err;

    std::scoped_lock lock(desktop_configuration_lock_);
    desktop_configuration_ = MacDesktopConfiguration::getCurrent(
        MacDesktopConfiguration::TopLeftOrigin);
}

//--------------------------------------------------------------------------------------------------
DesktopConfigurationMonitor::~DesktopConfigurationMonitor()
{
    CGError err = CGDisplayRemoveReconfigurationCallback(
        DesktopConfigurationMonitor::displaysReconfiguredCallback, this);
    if (err != kCGErrorSuccess)
        LOG(LS_ERROR) << "CGDisplayRemoveReconfigurationCallback " << err;
}

//--------------------------------------------------------------------------------------------------
MacDesktopConfiguration DesktopConfigurationMonitor::desktopConfiguration()
{
    std::scoped_lock lock(desktop_configuration_lock_);
    return desktop_configuration_;
}

//--------------------------------------------------------------------------------------------------
// static
// This method may be called on any system thread.
void DesktopConfigurationMonitor::displaysReconfiguredCallback(
    CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void* user_parameter)
{
    DesktopConfigurationMonitor* monitor =
        reinterpret_cast<DesktopConfigurationMonitor*>(user_parameter);
    monitor->displaysReconfigured(display, flags);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigurationMonitor::displaysReconfigured(
    CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags)
{
    LOG(LS_INFO) << "DisplaysReconfigured: DisplayID " << display << "; ChangeSummaryFlags " << flags;

    if (flags & kCGDisplayBeginConfigurationFlag)
    {
        reconfiguring_displays_.insert(display);
        return;
    }

    reconfiguring_displays_.erase(display);

    if (reconfiguring_displays_.empty())
    {
        std::scoped_lock lock(desktop_configuration_lock_);
        desktop_configuration_ = MacDesktopConfiguration::getCurrent(
            MacDesktopConfiguration::TopLeftOrigin);
    }
}

} // namespace base
