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

#include "base/linux/libxfixes.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libXfixes declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XFixesQueryExtension) g_query_extension = nullptr;
decltype(&XFixesGetCursorImage) g_get_cursor_image = nullptr;
decltype(&XFixesSelectCursorInput) g_select_cursor_input = nullptr;
decltype(&XFixesCreateRegion) g_create_region = nullptr;
decltype(&XFixesDestroyRegion) g_destroy_region = nullptr;
decltype(&XFixesFetchRegionAndBounds) g_fetch_region_and_bounds = nullptr;
decltype(&XFixesSelectSelectionInput) g_select_selection_input = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXfixes::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXfixes.so.3", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXfixes.so.3:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_query_extension =
        reinterpret_cast<decltype(g_query_extension)>(dlsym(g_handle, "XFixesQueryExtension"));
    g_get_cursor_image =
        reinterpret_cast<decltype(g_get_cursor_image)>(dlsym(g_handle, "XFixesGetCursorImage"));
    g_select_cursor_input =
        reinterpret_cast<decltype(g_select_cursor_input)>(dlsym(g_handle, "XFixesSelectCursorInput"));
    g_create_region =
        reinterpret_cast<decltype(g_create_region)>(dlsym(g_handle, "XFixesCreateRegion"));
    g_destroy_region =
        reinterpret_cast<decltype(g_destroy_region)>(dlsym(g_handle, "XFixesDestroyRegion"));
    g_fetch_region_and_bounds = reinterpret_cast<decltype(g_fetch_region_and_bounds)>(
        dlsym(g_handle, "XFixesFetchRegionAndBounds"));
    g_select_selection_input = reinterpret_cast<decltype(g_select_selection_input)>(
        dlsym(g_handle, "XFixesSelectSelectionInput"));

    if (!g_query_extension || !g_get_cursor_image || !g_select_cursor_input || !g_create_region ||
        !g_destroy_region || !g_fetch_region_and_bounds || !g_select_selection_input)
    {
        LOG(ERROR) << "Unable to resolve libXfixes symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibXfixes::queryExtension(Display* display, int* event_base, int* error_base)
{
    if (!ensureLoaded())
        return 0;
    return g_query_extension(display, event_base, error_base);
}

//--------------------------------------------------------------------------------------------------
// static
XFixesCursorImage* LibXfixes::getCursorImage(Display* display)
{
    if (!ensureLoaded())
        return nullptr;
    return g_get_cursor_image(display);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXfixes::selectCursorInput(Display* display, Window window, unsigned long mask)
{
    if (!ensureLoaded())
        return;
    g_select_cursor_input(display, window, mask);
}

//--------------------------------------------------------------------------------------------------
// static
XserverRegion LibXfixes::createRegion(Display* display, XRectangle* rectangles, int nrectangles)
{
    if (!ensureLoaded())
        return 0;
    return g_create_region(display, rectangles, nrectangles);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXfixes::destroyRegion(Display* display, XserverRegion region)
{
    if (!ensureLoaded())
        return;
    g_destroy_region(display, region);
}

//--------------------------------------------------------------------------------------------------
// static
XRectangle* LibXfixes::fetchRegionAndBounds(
    Display* display, XserverRegion region, int* nrectangles, XRectangle* bounds)
{
    if (!ensureLoaded())
        return nullptr;
    return g_fetch_region_and_bounds(display, region, nrectangles, bounds);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXfixes::selectSelectionInput(Display* display, Window window, Atom selection, unsigned long mask)
{
    if (!ensureLoaded())
        return;
    g_select_selection_input(display, window, selection, mask);
}
