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

#include "base/linux/libxrandr.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libXrandr declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XRRQueryExtension) g_query_extension = nullptr;
decltype(&XRRQueryVersion) g_query_version = nullptr;
decltype(&XRRSelectInput) g_select_input = nullptr;
decltype(&XRRUpdateConfiguration) g_update_configuration = nullptr;
decltype(&XRRGetMonitors) g_get_monitors = nullptr;
decltype(&XRRFreeMonitors) g_free_monitors = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXrandr::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXrandr.so.2", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXrandr.so.2:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_query_extension =
        reinterpret_cast<decltype(g_query_extension)>(dlsym(g_handle, "XRRQueryExtension"));
    g_query_version =
        reinterpret_cast<decltype(g_query_version)>(dlsym(g_handle, "XRRQueryVersion"));
    g_select_input =
        reinterpret_cast<decltype(g_select_input)>(dlsym(g_handle, "XRRSelectInput"));
    g_update_configuration =
        reinterpret_cast<decltype(g_update_configuration)>(dlsym(g_handle, "XRRUpdateConfiguration"));

    // Monitor enumeration (XRandR 1.5+). Optional: resolved here so callers reach it through the
    // library handle rather than the global symbol namespace (where it is not visible), but a missing
    // symbol on an older library must not fail the whole load.
    g_get_monitors =
        reinterpret_cast<decltype(g_get_monitors)>(dlsym(g_handle, "XRRGetMonitors"));
    g_free_monitors =
        reinterpret_cast<decltype(g_free_monitors)>(dlsym(g_handle, "XRRFreeMonitors"));

    if (!g_query_extension || !g_query_version || !g_select_input || !g_update_configuration)
    {
        LOG(ERROR) << "Unable to resolve libXrandr symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibXrandr::queryExtension(Display* display, int* event_base, int* error_base)
{
    if (!ensureLoaded())
        return 0;
    return g_query_extension(display, event_base, error_base);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXrandr::queryVersion(Display* display, int* major, int* minor)
{
    if (!ensureLoaded())
        return 0;
    return g_query_version(display, major, minor);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXrandr::selectInput(Display* display, Window window, int mask)
{
    if (!ensureLoaded())
        return;
    g_select_input(display, window, mask);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXrandr::updateConfiguration(XEvent* event)
{
    if (!ensureLoaded())
        return 0;
    return g_update_configuration(event);
}

//--------------------------------------------------------------------------------------------------
// static
XRRMonitorInfo* LibXrandr::getMonitors(Display* display, Window window, int get_active, int* nmonitors)
{
    if (!ensureLoaded() || !g_get_monitors)
        return nullptr;
    return g_get_monitors(display, window, get_active, nmonitors);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXrandr::freeMonitors(XRRMonitorInfo* monitors)
{
    if (!ensureLoaded() || !g_free_monitors)
        return;
    g_free_monitors(monitors);
}
