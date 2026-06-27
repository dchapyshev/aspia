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

#include "host/linux/wayland_compositor_source.h"

#include "base/scoped_qpointer.h"
#include "host/linux/mutter_screen_cast.h"
#include "host/linux/wayland_portal.h"

//--------------------------------------------------------------------------------------------------
WaylandCompositorSource::WaylandCompositorSource(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
bool WaylandCompositorSource::isAvailable(uid_t session_uid)
{
    // Mutter is probed by a throwaway instance (it connects to the session bus and checks the
    // interface); the portal exposes a static check.
    MutterScreenCast mutter(session_uid);
    if (mutter.isAvailable())
        return true;

    return WaylandPortal::isScreenCastAvailable(session_uid);
}

//--------------------------------------------------------------------------------------------------
// static
WaylandCompositorSource* WaylandCompositorSource::create(uid_t session_uid, QObject* parent)
{
    // Mutter ScreenCast first: it captures without a permission dialog (and so works on the login
    // screen). The portal is the generic fallback for wlroots and other compositors.
    ScopedQPointer<MutterScreenCast> mutter(new MutterScreenCast(session_uid, parent));
    if (mutter->isAvailable())
        return mutter.release();

    if (WaylandPortal::isScreenCastAvailable(session_uid))
        return new WaylandPortal(session_uid, parent);

    return nullptr;
}
