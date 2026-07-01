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

// base/logging.h must precede the <systemd/...> headers: sd-journal.h pulls in <syslog.h>, whose
// LOG_INFO/LOG_WARNING macros would clobber the logging severity constants.
#include "base/logging.h"

#include <systemd/sd-journal.h>
#include <systemd/sd-login.h>

#include <dlfcn.h>

namespace {

// Pointer types are taken from the real libsystemd declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&sd_seat_get_active) g_seat_get_active = nullptr;
decltype(&sd_session_get_vt) g_session_get_vt = nullptr;
decltype(&sd_session_get_class) g_session_get_class = nullptr;
decltype(&sd_session_get_type) g_session_get_type = nullptr;
decltype(&sd_login_monitor_new) g_login_monitor_new = nullptr;
decltype(&sd_login_monitor_unref) g_login_monitor_unref = nullptr;
decltype(&sd_login_monitor_get_fd) g_login_monitor_get_fd = nullptr;
decltype(&sd_login_monitor_flush) g_login_monitor_flush = nullptr;

decltype(&sd_journal_open) g_journal_open = nullptr;
decltype(&sd_journal_close) g_journal_close = nullptr;
decltype(&sd_journal_add_match) g_journal_add_match = nullptr;
decltype(&sd_journal_seek_head) g_journal_seek_head = nullptr;
decltype(&sd_journal_seek_tail) g_journal_seek_tail = nullptr;
decltype(&sd_journal_next) g_journal_next = nullptr;
decltype(&sd_journal_previous) g_journal_previous = nullptr;
decltype(&sd_journal_get_data) g_journal_get_data = nullptr;
decltype(&sd_journal_get_realtime_usec) g_journal_get_realtime_usec = nullptr;
decltype(&sd_journal_seek_cursor) g_journal_seek_cursor = nullptr;
decltype(&sd_journal_get_cursor) g_journal_get_cursor = nullptr;
decltype(&sd_journal_test_cursor) g_journal_test_cursor = nullptr;

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
    g_session_get_type =
        reinterpret_cast<decltype(g_session_get_type)>(dlsym(g_handle, "sd_session_get_type"));
    g_login_monitor_new =
        reinterpret_cast<decltype(g_login_monitor_new)>(dlsym(g_handle, "sd_login_monitor_new"));
    g_login_monitor_unref =
        reinterpret_cast<decltype(g_login_monitor_unref)>(dlsym(g_handle, "sd_login_monitor_unref"));
    g_login_monitor_get_fd =
        reinterpret_cast<decltype(g_login_monitor_get_fd)>(dlsym(g_handle, "sd_login_monitor_get_fd"));
    g_login_monitor_flush =
        reinterpret_cast<decltype(g_login_monitor_flush)>(dlsym(g_handle, "sd_login_monitor_flush"));

    g_journal_open =
        reinterpret_cast<decltype(g_journal_open)>(dlsym(g_handle, "sd_journal_open"));
    g_journal_close =
        reinterpret_cast<decltype(g_journal_close)>(dlsym(g_handle, "sd_journal_close"));
    g_journal_add_match =
        reinterpret_cast<decltype(g_journal_add_match)>(dlsym(g_handle, "sd_journal_add_match"));
    g_journal_seek_head =
        reinterpret_cast<decltype(g_journal_seek_head)>(dlsym(g_handle, "sd_journal_seek_head"));
    g_journal_seek_tail =
        reinterpret_cast<decltype(g_journal_seek_tail)>(dlsym(g_handle, "sd_journal_seek_tail"));
    g_journal_next =
        reinterpret_cast<decltype(g_journal_next)>(dlsym(g_handle, "sd_journal_next"));
    g_journal_previous =
        reinterpret_cast<decltype(g_journal_previous)>(dlsym(g_handle, "sd_journal_previous"));
    g_journal_get_data =
        reinterpret_cast<decltype(g_journal_get_data)>(dlsym(g_handle, "sd_journal_get_data"));
    g_journal_get_realtime_usec = reinterpret_cast<decltype(g_journal_get_realtime_usec)>(
        dlsym(g_handle, "sd_journal_get_realtime_usec"));
    g_journal_seek_cursor =
        reinterpret_cast<decltype(g_journal_seek_cursor)>(dlsym(g_handle, "sd_journal_seek_cursor"));
    g_journal_get_cursor =
        reinterpret_cast<decltype(g_journal_get_cursor)>(dlsym(g_handle, "sd_journal_get_cursor"));
    g_journal_test_cursor =
        reinterpret_cast<decltype(g_journal_test_cursor)>(dlsym(g_handle, "sd_journal_test_cursor"));

    if (!g_seat_get_active || !g_session_get_vt || !g_session_get_class || !g_session_get_type ||
        !g_login_monitor_new || !g_login_monitor_unref || !g_login_monitor_get_fd ||
        !g_login_monitor_flush || !g_journal_open || !g_journal_close || !g_journal_add_match ||
        !g_journal_seek_head || !g_journal_seek_tail || !g_journal_next || !g_journal_previous ||
        !g_journal_get_data || !g_journal_get_realtime_usec || !g_journal_seek_cursor ||
        !g_journal_get_cursor || !g_journal_test_cursor)
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
int LibSystemd::sessionGetType(const char* session, char** type)
{
    if (!ensureLoaded())
        return -1;
    return g_session_get_type(session, type);
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

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalOpen(sd_journal** journal)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_open(journal, SD_JOURNAL_LOCAL_ONLY);
}

//--------------------------------------------------------------------------------------------------
// static
void LibSystemd::journalClose(sd_journal* journal)
{
    if (!ensureLoaded())
        return;
    g_journal_close(journal);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalAddMatch(sd_journal* journal, const void* data, size_t size)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_add_match(journal, data, size);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalSeekHead(sd_journal* journal)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_seek_head(journal);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalSeekTail(sd_journal* journal)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_seek_tail(journal);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalNext(sd_journal* journal)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_next(journal);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalPrevious(sd_journal* journal)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_previous(journal);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalGetData(sd_journal* journal, const char* field, const void** data, size_t* length)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_get_data(journal, field, data, length);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalGetRealtimeUsec(sd_journal* journal, uint64_t* usec)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_get_realtime_usec(journal, usec);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalSeekCursor(sd_journal* journal, const char* cursor)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_seek_cursor(journal, cursor);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalGetCursor(sd_journal* journal, char** cursor)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_get_cursor(journal, cursor);
}

//--------------------------------------------------------------------------------------------------
// static
int LibSystemd::journalTestCursor(sd_journal* journal, const char* cursor)
{
    if (!ensureLoaded())
        return -1;
    return g_journal_test_cursor(journal, cursor);
}
