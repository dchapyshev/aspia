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

#ifndef BASE_LINUX_WAYLAND_OUTPUT_LAYOUT_H
#define BASE_LINUX_WAYLAND_OUTPUT_LAYOUT_H

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>

// Reads the compositor's logical monitor layout (positions and logical sizes, which include the
// possibly fractional scale) through the Wayland wl_output + xdg-output protocols. A privileged
// process can connect to another user's compositor socket - Wayland has no per-uid authentication on
// the socket the way D-Bus does - so the greeter's layout is readable from the root agent, which DRM
// (no logical layout) and D-Bus (blocked for root) cannot provide. DE-independent: any Wayland
// compositor exposes these interfaces.
class WaylandOutputLayout
{
public:
    struct Output
    {
        QString name;     // connector name as the compositor reports it (e.g. "eDP-1")
        QSize physical;   // current mode size in physical pixels
        QRect logical;    // position and size in the compositor's logical coordinate space
    };

    // Connects to the Wayland socket at |socket_path| and returns the outputs, or an empty list on
    // failure (not Wayland, socket missing, or the protocols are unavailable).
    static QList<Output> query(const QString& socket_path);
};

#endif // BASE_LINUX_WAYLAND_OUTPUT_LAYOUT_H
