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

#include "base/linux/libxext.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libXext declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XShmQueryVersion) g_shm_query_version = nullptr;
decltype(&XShmCreateImage) g_shm_create_image = nullptr;
decltype(&XShmAttach) g_shm_attach = nullptr;
decltype(&XShmPixmapFormat) g_shm_pixmap_format = nullptr;
decltype(&XShmCreatePixmap) g_shm_create_pixmap = nullptr;
decltype(&XShmGetImage) g_shm_get_image = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXext::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXext.so.6", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXext.so.6:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_shm_query_version =
        reinterpret_cast<decltype(g_shm_query_version)>(dlsym(g_handle, "XShmQueryVersion"));
    g_shm_create_image =
        reinterpret_cast<decltype(g_shm_create_image)>(dlsym(g_handle, "XShmCreateImage"));
    g_shm_attach = reinterpret_cast<decltype(g_shm_attach)>(dlsym(g_handle, "XShmAttach"));
    g_shm_pixmap_format =
        reinterpret_cast<decltype(g_shm_pixmap_format)>(dlsym(g_handle, "XShmPixmapFormat"));
    g_shm_create_pixmap =
        reinterpret_cast<decltype(g_shm_create_pixmap)>(dlsym(g_handle, "XShmCreatePixmap"));
    g_shm_get_image = reinterpret_cast<decltype(g_shm_get_image)>(dlsym(g_handle, "XShmGetImage"));

    if (!g_shm_query_version || !g_shm_create_image || !g_shm_attach || !g_shm_pixmap_format ||
        !g_shm_create_pixmap || !g_shm_get_image)
    {
        LOG(ERROR) << "Unable to resolve libXext symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibXext::shmQueryVersion(Display* display, int* major, int* minor, int* shared_pixmaps)
{
    if (!ensureLoaded())
        return 0;
    return g_shm_query_version(display, major, minor, shared_pixmaps);
}

//--------------------------------------------------------------------------------------------------
// static
XImage* LibXext::shmCreateImage(
    Display* display, Visual* visual, unsigned int depth, int format, char* data,
    XShmSegmentInfo* shminfo, unsigned int width, unsigned int height)
{
    if (!ensureLoaded())
        return nullptr;
    return g_shm_create_image(display, visual, depth, format, data, shminfo, width, height);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXext::shmAttach(Display* display, XShmSegmentInfo* shminfo)
{
    if (!ensureLoaded())
        return 0;
    return g_shm_attach(display, shminfo);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXext::shmPixmapFormat(Display* display)
{
    if (!ensureLoaded())
        return 0;
    return g_shm_pixmap_format(display);
}

//--------------------------------------------------------------------------------------------------
// static
Pixmap LibXext::shmCreatePixmap(
    Display* display, Drawable drawable, char* data, XShmSegmentInfo* shminfo,
    unsigned int width, unsigned int height, unsigned int depth)
{
    if (!ensureLoaded())
        return 0;
    return g_shm_create_pixmap(display, drawable, data, shminfo, width, height, depth);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXext::shmGetImage(
    Display* display, Drawable drawable, XImage* image, int x, int y, unsigned long plane_mask)
{
    if (!ensureLoaded())
        return 0;
    return g_shm_get_image(display, drawable, image, x, y, plane_mask);
}
