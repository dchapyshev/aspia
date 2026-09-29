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

#ifndef COMMON_MAC_DOCK_ICON_H
#define COMMON_MAC_DOCK_ICON_H

// Shows or hides the icon of the application in the Dock. The application is still a menu bar agent in
// its Info.plist and starts without the icon.
void setDockIconVisible(bool visible);

#endif // COMMON_MAC_DOCK_ICON_H
