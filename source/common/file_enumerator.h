//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_FILE_ENUMERATOR_H
#define COMMON_FILE_ENUMERATOR_H

#include "base/macros_magic.h"
#include "build/build_config.h"
#include "proto/file_transfer.h"

#include <QString>
#include <filesystem>

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace common {

class FileEnumerator
{
public:
    class FileInfo
    {
    public:
        FileInfo();
        ~FileInfo() = default;

        bool isDirectory() const;
        QString name() const;
        qint64 size() const;
        time_t lastWriteTime() const;

    private:
        friend class FileEnumerator;
#if defined(OS_WIN)
        WIN32_FIND_DATA find_data_;
#endif // defined(OS_WIN)

#if defined(OS_POSIX)
    std::filesystem::directory_iterator it_;
#endif // defined(OS_POSIX)
    };

    explicit FileEnumerator(const QString& root_path);
    ~FileEnumerator();

    const FileInfo& fileInfo() const { return file_info_; }
    bool isAtEnd() const;
    void advance();

    proto::FileError errorCode() const { return error_code_; }

private:
    proto::FileError error_code_ = proto::FILE_ERROR_SUCCESS;
    FileInfo file_info_;

#if defined(Q_OS_WINDOWS)
    HANDLE find_handle_ = INVALID_HANDLE_VALUE;
#endif // defined(Q_OS_WINDOWS)

    DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

}  // namespace common

#endif // COMMON_FILE_ENUMERATOR_H
