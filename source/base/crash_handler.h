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

#ifndef BASE_CRASH_HANDLER_H
#define BASE_CRASH_HANDLER_H

#include <QString>

// Initializes the symbolizer and installs the platform crash handler.
// |dump_file_prefix| is used as the leading part of the minidump file name.
// On Windows an unhandled SEH filter is set that writes a stack trace to the
// log and produces a minidump file. On Linux and macOS handlers for the fatal
// signals are installed that write a crash report to the log and re-raise the
// signal so the OS produces a core dump. On Android this is a no-op.
void installCrashHandler(const QString& dump_file_prefix);

// Provides the active log file native descriptor so the crash handler can append
// crash information to it directly, bypassing CRT and user-space locking. The value
// is platform-specific (see LoggingFile::nativeHandle): a Win32 HANDLE cast to qintptr
// on Windows, a POSIX file descriptor elsewhere. Pass -1 to detach.
// No-op on platforms where the crash handler is not implemented.
void setCrashLogHandle(qintptr handle);

// Returns a symbolized stack trace of the current call stack as a string.
// |skip_frames| controls how many innermost frames to skip (defaults to the
// caller of this function).
QString captureStackTrace(int skip_frames = 1);

#endif // BASE_CRASH_HANDLER_H
