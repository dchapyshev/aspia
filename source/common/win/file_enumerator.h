//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__WIN__FILE_ENUMERATOR_H
#define COMMON__WIN__FILE_ENUMERATOR_H

#include "base/macros_magic.h"
#include "proto/file_transfer.pb.h"

#include <filesystem>

#include <Windows.h>

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
        std::filesystem::path name() const;
        int64_t size() const;
        time_t lastWriteTime() const;

    private:
        friend class FileEnumerator;
        WIN32_FIND_DATA find_data_;
    };

    FileEnumerator(const std::filesystem::path& root_path);
    ~FileEnumerator();

    const FileInfo& fileInfo() const { return file_info_; }
    bool isAtEnd() const;
    void advance();

    proto::FileError errorCode() const { return error_code_; }

private:
    proto::FileError error_code_ = proto::FILE_ERROR_SUCCESS;
    HANDLE find_handle_ = INVALID_HANDLE_VALUE;
    FileInfo file_info_;

    DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

}  // namespace common

#endif // COMMON__WIN__FILE_ENUMERATOR_H
