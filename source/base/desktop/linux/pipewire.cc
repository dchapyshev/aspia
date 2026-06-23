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

#include "base/desktop/linux/pipewire.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

void* g_handle = nullptr;
bool g_load_failed = false;
PipeWireApi g_api;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool PipeWire::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libpipewire-0.3.so.0", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libpipewire-0.3.so.0:" << dlerror();
        g_load_failed = true;
        return false;
    }

    bool ok = true;

#define ASPIA_PIPEWIRE_RESOLVE(sym)                                                  \
    g_api.sym = reinterpret_cast<decltype(g_api.sym)>(dlsym(g_handle, #sym));        \
    if (!g_api.sym)                                                                  \
    {                                                                                \
        LOG(ERROR) << "Unable to resolve pipewire symbol:" << #sym;                  \
        ok = false;                                                                  \
    }
    ASPIA_PIPEWIRE_SYMBOLS(ASPIA_PIPEWIRE_RESOLVE)
#undef ASPIA_PIPEWIRE_RESOLVE

    if (!ok)
    {
        dlclose(g_handle);
        g_handle = nullptr;
        g_api = PipeWireApi();
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
const PipeWireApi* PipeWire::api()
{
    // Null until the library has been loaded successfully, so callers can detect that.
    return g_handle ? &g_api : nullptr;
}
