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

#ifndef BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_H
#define BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_H

#include "base/desktop/geometry.h"

#include <vector>

#include <ApplicationServices/ApplicationServices.h>

namespace base {

// Describes the configuration of a specific display.
struct MacDisplayConfiguration
{
    MacDisplayConfiguration();
    MacDisplayConfiguration(const MacDisplayConfiguration& other);
    MacDisplayConfiguration(MacDisplayConfiguration&& other);
    ~MacDisplayConfiguration();

    MacDisplayConfiguration& operator=(const MacDisplayConfiguration& other);
    MacDisplayConfiguration& operator=(MacDisplayConfiguration&& other);

    // Cocoa identifier for this display.
    CGDirectDisplayID id = 0;

    // Bounds of this display in Density-Independent Pixels (DIPs).
    Rect bounds;

    // Bounds of this display in physical pixels.
    Rect pixel_bounds;

    // Scale factor from DIPs to physical pixels.
    float dip_to_pixel_scale = 1.0f;

    // Display type, built-in or external.
    bool is_builtin = false;

    // Display primary or not.
    bool is_primary_ = false;
};

typedef std::vector<MacDisplayConfiguration> MacDisplayConfigurations;

// Describes the configuration of the whole desktop.
struct MacDesktopConfiguration
{
    // Used to request bottom-up or top-down coordinates.
    enum Origin { BottomLeftOrigin, TopLeftOrigin };

    MacDesktopConfiguration();
    MacDesktopConfiguration(const MacDesktopConfiguration& other);
    MacDesktopConfiguration(MacDesktopConfiguration&& other);
    ~MacDesktopConfiguration();

    MacDesktopConfiguration& operator=(const MacDesktopConfiguration& other);
    MacDesktopConfiguration& operator=(MacDesktopConfiguration&& other);

    // Returns the desktop & display configurations.
    // If BottomLeftOrigin is used, the output is in Cocoa-style "bottom-up" (the origin is the
    // bottom-left of the primary monitor, and coordinates increase as you move up the screen).
    // Otherwise, the configuration will be converted to follow top-left coordinate system as
    // Windows and X11.
    static MacDesktopConfiguration getCurrent(Origin origin);

    // Returns true if the given desktop configuration equals this one.
    bool equals(const MacDesktopConfiguration& other);

    // If `id` corresponds to the built-in display, return its configuration, otherwise return the
    // configuration for the display with the specified id, or nullptr if no such display exists.
    const MacDisplayConfiguration* findDisplayConfigurationById(CGDirectDisplayID id);

    // Bounds of the desktop excluding monitors with DPI settings different from the main monitor.
    // In Density-Independent Pixels (DIPs).
    Rect bounds;

    // Same as bounds, but expressed in physical pixels.
    Rect pixel_bounds;

    // Scale factor from DIPs to physical pixels.
    float dip_to_pixel_scale = 1.0f;

    // Configurations of the displays making up the desktop area.
    MacDisplayConfigurations displays;
};

} // namespace base

#endif // BASE_DESKTOP_MAC_DESKTOP_CONFIGURATION_H
