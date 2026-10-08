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

#include "host/linux/x_window_util.h"

#include <QGuiApplication>
#include <QList>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>

#include <cstdlib>
#include <memory>

namespace {

//--------------------------------------------------------------------------------------------------
struct XcbReplyDeleter
{
    void operator()(void* reply) const { free(reply); }
};

//--------------------------------------------------------------------------------------------------
template <typename T>
using ScopedXcbReply = std::unique_ptr<T, XcbReplyDeleter>;

//--------------------------------------------------------------------------------------------------
xcb_atom_t internAtom(xcb_connection_t* connection, const QByteArray& name)
{
    ScopedXcbReply<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(connection,
        xcb_intern_atom(connection, false, static_cast<quint16>(name.size()), name.constData()), nullptr));
    return reply ? reply->atom : XCB_ATOM_NONE;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
void XWindowUtil::removeTakeFocusProtocol(WId window)
{
    // Qt gives every window WM_TAKE_FOCUS, and with the input hint off this is the Globally Active model
    // of ICCCM. labwc lists such a window in the taskbar when it also has the NORMAL type, which Qt adds
    // to any type. Without the protocol the window has the No Input model and is never listed.
    auto* x11_app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11_app)
        return;

    xcb_connection_t* connection = x11_app->connection();
    if (!connection)
        return;

    const xcb_atom_t protocols_atom = internAtom(connection, "WM_PROTOCOLS");
    const xcb_atom_t take_focus_atom = internAtom(connection, "WM_TAKE_FOCUS");
    if (protocols_atom == XCB_ATOM_NONE || take_focus_atom == XCB_ATOM_NONE)
        return;

    const xcb_window_t xcb_window = static_cast<xcb_window_t>(window);

    ScopedXcbReply<xcb_get_property_reply_t> reply(xcb_get_property_reply(connection,
        xcb_get_property(connection, false, xcb_window, protocols_atom, XCB_ATOM_ATOM, 0, 32), nullptr));
    if (!reply)
        return;

    const xcb_atom_t* atoms = static_cast<const xcb_atom_t*>(xcb_get_property_value(reply.get()));
    const int count = xcb_get_property_value_length(reply.get()) / static_cast<int>(sizeof(xcb_atom_t));

    QList<xcb_atom_t> protocols;
    for (int i = 0; i < count; ++i)
    {
        if (atoms[i] != take_focus_atom)
            protocols.append(atoms[i]);
    }

    if (protocols.size() == count)
        return;

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xcb_window, protocols_atom, XCB_ATOM_ATOM, 32,
                        static_cast<quint32>(protocols.size()), protocols.constData());
    xcb_flush(connection);
}
