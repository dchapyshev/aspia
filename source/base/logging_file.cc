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

#include "base/logging_file.h"

#include <QString>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif

//--------------------------------------------------------------------------------------------------
LoggingFile::LoggingFile()
#if defined(Q_OS_WINDOWS)
    : handle_(INVALID_HANDLE_VALUE)
#endif
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
LoggingFile::~LoggingFile()
{
    close();
}

//--------------------------------------------------------------------------------------------------
bool LoggingFile::open(const QString& path)
{
#if defined(Q_OS_WINDOWS)
    close();

    static const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    static const DWORD desired_access = FILE_APPEND_DATA | SYNCHRONIZE;

    handle_ = CreateFileW(qUtf16Printable(path), desired_access, share_mode, nullptr, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE)
        return false;
    return true;
#else
    QMutexLocker lock(&mutex_);

    if (file_.isOpen())
        file_.close();

    file_.setFileName(path);
    return file_.open(QFile::WriteOnly | QFile::Append | QFile::Text);
#endif
}

//--------------------------------------------------------------------------------------------------
void LoggingFile::close()
{
#if defined(Q_OS_WINDOWS)
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    QMutexLocker lock(&mutex_);
    file_.close();
#endif
}

//--------------------------------------------------------------------------------------------------
void LoggingFile::write(const char* data, qint64 size)
{
#if defined(Q_OS_WINDOWS)
    if (handle_ == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    // FILE_APPEND_DATA guarantees the kernel atomically appends to the end. No mutex required.
    WriteFile(handle_, data, static_cast<DWORD>(size), &written, nullptr);
#else
    QMutexLocker lock(&mutex_);
    if (!file_.isOpen())
        return;
    file_.write(data, size);
    file_.flush();
#endif
}

//--------------------------------------------------------------------------------------------------
bool LoggingFile::isOpen() const
{
#if defined(Q_OS_WINDOWS)
    return handle_ != INVALID_HANDLE_VALUE;
#else
    QMutexLocker lock(&mutex_);
    return file_.isOpen();
#endif
}

//--------------------------------------------------------------------------------------------------
qintptr LoggingFile::nativeHandle() const
{
#if defined(Q_OS_WINDOWS)
    if (handle_ == INVALID_HANDLE_VALUE)
        return -1;
    return reinterpret_cast<qintptr>(handle_);
#else
    QMutexLocker lock(&mutex_);
    if (!file_.isOpen())
        return -1;
    return static_cast<qintptr>(file_.handle());
#endif
}
