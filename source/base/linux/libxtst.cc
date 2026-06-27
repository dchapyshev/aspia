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

#include "base/linux/libxtst.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/linux/x11_headers.h"

namespace {

// Pointer types are taken from the real libXtst declarations; only the addresses are resolved dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XTestQueryExtension) g_query_extension = nullptr;
decltype(&XTestGrabControl) g_grab_control = nullptr;
decltype(&XTestFakeKeyEvent) g_fake_key_event = nullptr;
decltype(&XTestFakeButtonEvent) g_fake_button_event = nullptr;
decltype(&XTestFakeMotionEvent) g_fake_motion_event = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXtst::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXtst.so.6", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXtst.so.6:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_query_extension =
        reinterpret_cast<decltype(g_query_extension)>(dlsym(g_handle, "XTestQueryExtension"));
    g_grab_control =
        reinterpret_cast<decltype(g_grab_control)>(dlsym(g_handle, "XTestGrabControl"));
    g_fake_key_event =
        reinterpret_cast<decltype(g_fake_key_event)>(dlsym(g_handle, "XTestFakeKeyEvent"));
    g_fake_button_event =
        reinterpret_cast<decltype(g_fake_button_event)>(dlsym(g_handle, "XTestFakeButtonEvent"));
    g_fake_motion_event =
        reinterpret_cast<decltype(g_fake_motion_event)>(dlsym(g_handle, "XTestFakeMotionEvent"));

    if (!g_query_extension || !g_grab_control || !g_fake_key_event || !g_fake_button_event ||
        !g_fake_motion_event)
    {
        LOG(ERROR) << "Unable to resolve libXtst symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibXtst::queryExtension(Display* display, int* event_base, int* error_base, int* major, int* minor)
{
    if (!ensureLoaded())
        return 0;
    return g_query_extension(display, event_base, error_base, major, minor);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXtst::grabControl(Display* display, int impervious)
{
    if (!ensureLoaded())
        return 0;
    return g_grab_control(display, impervious);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXtst::fakeKeyEvent(Display* display, unsigned int keycode, int is_press, unsigned long delay)
{
    if (!ensureLoaded())
        return 0;
    return g_fake_key_event(display, keycode, is_press, delay);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXtst::fakeButtonEvent(Display* display, unsigned int button, int is_press, unsigned long delay)
{
    if (!ensureLoaded())
        return 0;
    return g_fake_button_event(display, button, is_press, delay);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXtst::fakeMotionEvent(Display* display, int screen, int x, int y, unsigned long delay)
{
    if (!ensureLoaded())
        return 0;
    return g_fake_motion_event(display, screen, x, y, delay);
}
