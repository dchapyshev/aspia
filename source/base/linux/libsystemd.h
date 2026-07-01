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

#ifndef BASE_LINUX_LIBSYSTEMD_H
#define BASE_LINUX_LIBSYSTEMD_H

#include <QtClassHelperMacros>

#include <sys/types.h>

#include <cstddef>
#include <cstdint>

struct sd_journal;
struct sd_login_monitor;

// Thin wrapper over the subset of libsystemd (sd-login) used by the host. The library is loaded
// dynamically on first use and the resolved symbols are cached, so the binary does not link against
// libsystemd. Every method returns a failure result if the library or a symbol is unavailable.
class LibSystemd
{
    Q_DISABLE_COPY_MOVE(LibSystemd)

public:
    // Loads libsystemd and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int seatGetActive(const char* seat, char** session, uid_t* uid);
    static int sessionGetVt(const char* session, unsigned* vtnr);
    static int sessionGetClass(const char* session, char** clazz);
    static int sessionGetType(const char* session, char** type);

    static int loginMonitorNew(const char* category, sd_login_monitor** ret);
    static sd_login_monitor* loginMonitorUnref(sd_login_monitor* monitor);
    static int loginMonitorGetFd(sd_login_monitor* monitor);
    static int loginMonitorFlush(sd_login_monitor* monitor);

    static int journalOpen(sd_journal** journal);
    static void journalClose(sd_journal* journal);
    static int journalAddMatch(sd_journal* journal, const void* data, size_t size);
    static int journalSeekHead(sd_journal* journal);
    static int journalSeekTail(sd_journal* journal);
    static int journalNext(sd_journal* journal);
    static int journalPrevious(sd_journal* journal);
    static int journalGetData(sd_journal* journal, const char* field, const void** data, size_t* length);
    static int journalGetRealtimeUsec(sd_journal* journal, uint64_t* usec);
    static int journalSeekCursor(sd_journal* journal, const char* cursor);
    static int journalGetCursor(sd_journal* journal, char** cursor);
    static int journalTestCursor(sd_journal* journal, const char* cursor);
};

#endif // BASE_LINUX_LIBSYSTEMD_H
