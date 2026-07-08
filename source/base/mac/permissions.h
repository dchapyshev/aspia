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

#ifndef BASE_MAC_PERMISSIONS_H
#define BASE_MAC_PERMISSIONS_H

// Privacy (TCC) permissions the host relies on: Screen Recording for screen capture, Accessibility for
// input injection, Full Disk Access for file transfer to reach protected locations.
enum class MacPermission
{
    SCREEN_RECORDING,
    ACCESSIBILITY,
    FULL_DISK_ACCESS
};

// Whether |permission| is currently granted. Does not prompt.
bool hasMacPermission(MacPermission permission);

// Asks for |permission|: shows the system prompt where the API allows it, otherwise opens the relevant
// Privacy pane in System Settings. Screen Recording only takes effect after the application restarts.
// The system prompt for Screen Recording / Accessibility appears only once per application launch; use
// openMacPermissionSettings() for later attempts.
void requestMacPermission(MacPermission permission);

// Opens the System Settings Privacy pane for |permission| so the user can toggle the application there.
void openMacPermissionSettings(MacPermission permission);

#endif // BASE_MAC_PERMISSIONS_H
