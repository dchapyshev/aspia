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

#include "base/linux/libxdamage.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/linux/x11_headers.h"

namespace {

// Pointer types are taken from the real libXdamage declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XDamageQueryExtension) g_query_extension = nullptr;
decltype(&XDamageCreate) g_create = nullptr;
decltype(&XDamageDestroy) g_destroy = nullptr;
decltype(&XDamageSubtract) g_subtract = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXdamage::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXdamage.so.1", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXdamage.so.1:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_query_extension =
        reinterpret_cast<decltype(g_query_extension)>(dlsym(g_handle, "XDamageQueryExtension"));
    g_create = reinterpret_cast<decltype(g_create)>(dlsym(g_handle, "XDamageCreate"));
    g_destroy = reinterpret_cast<decltype(g_destroy)>(dlsym(g_handle, "XDamageDestroy"));
    g_subtract = reinterpret_cast<decltype(g_subtract)>(dlsym(g_handle, "XDamageSubtract"));

    if (!g_query_extension || !g_create || !g_destroy || !g_subtract)
    {
        LOG(ERROR) << "Unable to resolve libXdamage symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibXdamage::queryExtension(Display* display, int* event_base, int* error_base)
{
    if (!ensureLoaded())
        return 0;
    return g_query_extension(display, event_base, error_base);
}

//--------------------------------------------------------------------------------------------------
// static
unsigned long LibXdamage::create(Display* display, unsigned long drawable, int level)
{
    if (!ensureLoaded())
        return 0;
    return g_create(display, drawable, level);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXdamage::destroy(Display* display, unsigned long damage)
{
    if (!ensureLoaded())
        return;
    g_destroy(display, damage);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXdamage::subtract(Display* display, unsigned long damage, unsigned long repair, unsigned long parts)
{
    if (!ensureLoaded())
        return;
    g_subtract(display, damage, repair, parts);
}
