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

#ifndef BASE_DESKTOP_LINUX_WAYLAND_COMPOSITOR_SOURCE_H
#define BASE_DESKTOP_LINUX_WAYLAND_COMPOSITOR_SOURCE_H

#include <QObject>

#include <sys/types.h>

#include "base/desktop/linux/wayland_capture_source.h"
#include "base/desktop/linux/wayland_input_target.h"

// A compositor-provided source that supplies both a PipeWire capture stream and an input channel on
// Wayland, negotiated asynchronously. The concrete backends - GNOME Mutter ScreenCast (used directly,
// without a permission dialog) and the xdg-desktop-portal session (wlroots and others) - stay behind
// create(); the caller works through this interface only.
class WaylandCompositorSource
    : public QObject,
      public WaylandCaptureSource,
      public WaylandInputTarget
{
    Q_OBJECT

public:
    // Returns true if any compositor screen-cast backend is available on the active session of
    // |session_uid|.
    static bool isAvailable(uid_t session_uid);

    // Creates the best available backend (Mutter ScreenCast first, then the xdg-desktop-portal session),
    // or nullptr if none is available.
    static WaylandCompositorSource* create(uid_t session_uid, QObject* parent = nullptr);

    // Begins asynchronous negotiation of the capture stream; sig_started() reports the outcome.
    virtual void start() = 0;

signals:
    void sig_started(bool success);

protected:
    explicit WaylandCompositorSource(QObject* parent = nullptr);
};

#endif // BASE_DESKTOP_LINUX_WAYLAND_COMPOSITOR_SOURCE_H
