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

#include "base/desktop/x11/x_error_trap.h"

#include <cassert>
#include <cstddef>

namespace base {

namespace {

// TODO(sergeyu): This code is not thread safe. Fix it. Bug 2202.
static bool g_xserver_error_trap_enabled = false;
static int g_last_xserver_error_code = 0;

//--------------------------------------------------------------------------------------------------
int XServerErrorHandler(Display* display, XErrorEvent* error_event)
{
    assert(g_xserver_error_trap_enabled);
    g_last_xserver_error_code = error_event->error_code;
    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
XErrorTrap::XErrorTrap(Display* display)
    : original_error_handler_(NULL),
      enabled_(true)
{
    assert(!g_xserver_error_trap_enabled);
    original_error_handler_ = XSetErrorHandler(&XServerErrorHandler);
    g_xserver_error_trap_enabled = true;
    g_last_xserver_error_code = 0;
}

//--------------------------------------------------------------------------------------------------
int XErrorTrap::lastErrorAndDisable()
{
    enabled_ = false;

    assert(g_xserver_error_trap_enabled);
    XSetErrorHandler(original_error_handler_);
    g_xserver_error_trap_enabled = false;
    return g_last_xserver_error_code;
}

//--------------------------------------------------------------------------------------------------
XErrorTrap::~XErrorTrap()
{
    if (enabled_)
        lastErrorAndDisable();
}

}  // namespace webrtc
