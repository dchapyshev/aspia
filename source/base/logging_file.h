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

#ifndef BASE_LOGGING_FILE_H
#define BASE_LOGGING_FILE_H

#include <QtGlobal>

#if !defined(Q_OS_WINDOWS)
#include <QFile>
#include <QMutex>
#endif

class QString;

// Cross-platform log file abstraction.
//
// On Windows the file is opened via Win32 API with FILE_APPEND_DATA. The kernel guarantees that
// every WriteFile() is atomically appended to the end of the file, so concurrent writers do not
// corrupt each other and no user-space mutex is required. This matches the approach used by
// Chromium's base/logging.cc.
//
// On Linux/macOS the file is wrapped in a QFile guarded by an internal QMutex.
//
// The lifetime contract matches Chromium: open() is called once at logging init (before any
// worker thread starts logging) and close() once at shutdown (after worker threads have stopped).
// Concurrent open/close while writers are running is not supported.
class LoggingFile
{
public:
    LoggingFile();
    ~LoggingFile();

    bool open(const QString& path);
    void close();

    // Thread-safe append. On Windows lockless via FILE_APPEND_DATA, on POSIX guarded by mutex.
    void write(const char* data, qint64 size);

    bool isOpen() const;

    // Native file descriptor: Win32 HANDLE (cast to qintptr) on Windows, POSIX int fd elsewhere.
    // Returns -1 if not open. Intended for direct OS-level writes (e.g. crash handlers) that
    // need to bypass user-space locking and CRT buffering.
    qintptr nativeHandle() const;

private:
#if defined(Q_OS_WINDOWS)
    void* handle_;
#else
    mutable QMutex mutex_;
    QFile file_;
#endif

    Q_DISABLE_COPY_MOVE(LoggingFile)
};

#endif // BASE_LOGGING_FILE_H
