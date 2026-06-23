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

#include "base/linux/libsystemd.h"

#include <systemd/sd-login.h>

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libsystemd declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&sd_seat_get_active) g_seat_get_active = nullptr;
decltype(&sd_session_get_vt) g_session_get_vt = nullptr;
decltype(&sd_session_get_class) g_session_get_class = nullptr;
decltype(&sd_login_monitor_new) g_login_monitor_new = nullptr;
decltype(&sd_login_monitor_unref) g_login_monitor_unref = nullptr;
decltype(&sd_login_monitor_get_fd) g_login_monitor_get_fd = nullptr;
decltype(&sd_login_monitor_flush) g_login_monitor_flush = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibSystemd::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libsystemd.so.0", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libsystemd.so.0:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_seat_get_active =
        reinterpret_cast<decltype(g_seat_get_active)>(dlsym(g_handle, "sd_seat_get_active"));
    g_session_get_vt =
        reinterpret_cast<decltype(g_session_get_vt)>(dlsym(g_handle, "sd_session_get_vt"));
    g_session_get_class =
        reinterpret_cast<decltype(g_session_get_class)>(dlsym(g_handle, "sd_session_get_class"));
    g_login_monitor_new =
        reinterpret_cast<decltype(g_login_monitor_new)>(dlsym(g_handle, "sd_login_monitor_new"));
    g_login_monitor_unref =
        reinterpret_cast<decltype(g_login_monitor_unref)>(dlsym(g_handle, "sd_login_monitor_unref"));
    g_login_monitor_get_fd =
        reinterpret_cast<decltype(g_login_monitor_get_fd)>(dlsym(g_handle, "sd_login_monitor_get_fd"));
    g_login_monitor_flush =
        reinterpret_cast<decltype(g_login_monitor_flush)>(dlsym(g_handle, "sd_login_monitor_flush"));

    if (!g_seat_get_active || !g_session_get_vt || !g_session_get_class || !g_login_monitor_new ||
        !g_login_monitor_unref || !g_login_monitor_get_fd || !g_login_monitor_flush)
    {
        LOG(ERROR) << "Unable to resolve libsystemd symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::seatGetActive(const char* seat, char** session, uid_t* uid)
{
    if (!ensureLoaded())
        return -1;
    return g_seat_get_active(seat, session, uid);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::sessionGetVt(const char* session, unsigned* vtnr)
{
    if (!ensureLoaded())
        return -1;
    return g_session_get_vt(session, vtnr);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::sessionGetClass(const char* session, char** clazz)
{
    if (!ensureLoaded())
        return -1;
    return g_session_get_class(session, clazz);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::loginMonitorNew(const char* category, sd_login_monitor** ret)
{
    if (!ensureLoaded())
        return -1;
    return g_login_monitor_new(category, ret);
}

//--------------------------------------------------------------------------------------------------
// static
sd_login_monitor* LibSystemd::loginMonitorUnref(sd_login_monitor* monitor)
{
    if (!ensureLoaded())
        return nullptr;
    return g_login_monitor_unref(monitor);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::loginMonitorGetFd(sd_login_monitor* monitor)
{
    if (!ensureLoaded())
        return -1;
    return g_login_monitor_get_fd(monitor);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::loginMonitorFlush(sd_login_monitor* monitor)
{
    if (!ensureLoaded())
        return -1;
    return g_login_monitor_flush(monitor);
}
