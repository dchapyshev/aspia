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

namespace base {

// Initializes the symbolizer and installs the platform crash handler.
// On Windows an unhandled SEH filter is set that writes a stack trace to the
// log and produces a minidump file. On other platforms this is a no-op for now.
void installCrashHandler();

// Provides the active log file descriptor so the crash handler can append
// crash information to it directly (bypassing Qt locking). Pass -1 to detach.
void setCrashLogFileDescriptor(int fd);

// Returns a symbolized stack trace of the current call stack as a string.
// |skip_frames| controls how many innermost frames to skip (defaults to the
// caller of this function).
QString captureStackTrace(int skip_frames = 1);

} // namespace base

#endif // BASE_CRASH_HANDLER_H
