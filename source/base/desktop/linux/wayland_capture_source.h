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

#ifndef BASE_DESKTOP_LINUX_WAYLAND_CAPTURE_SOURCE_H
#define BASE_DESKTOP_LINUX_WAYLAND_CAPTURE_SOURCE_H

#include <QPoint>
#include <QSize>

#include <sys/types.h>

// Abstract provider of a PipeWire screen-capture stream on Wayland. Implemented by WaylandPortal (via
// xdg-desktop-portal, which asks the user for permission) and by MutterScreenCast (via the compositor's
// org.gnome.Mutter.ScreenCast D-Bus interface directly, without a permission dialog - used to capture
// the login screen). ScreenCapturerPipeWire consumes whichever source it is given.
class WaylandCaptureSource
{
public:
    virtual ~WaylandCaptureSource() = default;

    struct Stream
    {
        quint32 node_id = 0;
        QPoint position;
        QSize size;
    };

    virtual bool isStarted() const = 0;
    virtual const Stream& stream() const = 0;

    // File descriptor for pw_context_connect_fd(), or -1 when the consumer should connect to the
    // default PipeWire socket (the stream lives on the session's own PipeWire daemon).
    virtual int pipeWireFd() const = 0;

    // When pipeWireFd() is -1, the uid whose PipeWire daemon provides the stream. The root host's
    // environment does not point at the user's runtime dir, so the consumer connects to
    // /run/user/<uid>/pipewire-0 explicitly (root may open it). Returns (uid_t)-1 to use the default
    // socket from the environment.
    virtual uid_t pipeWireUid() const { return static_cast<uid_t>(-1); }
};

#endif // BASE_DESKTOP_LINUX_WAYLAND_CAPTURE_SOURCE_H
