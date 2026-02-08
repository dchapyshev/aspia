//
// SmartCafe Project
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

#ifndef BASE_FILES_FILE_ENUMERATOR_H
#define BASE_FILES_FILE_ENUMERATOR_H

#include <QString>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#else
#include <QDir>
#endif

namespace base {

// We use the native implementation of getting the list of files in a directory because QDir tries
// to resolve network location shortcuts when calling methods entryInfoList and entryList. If these
// network locations are unavailable, this causes large delays in getting the list of files.
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
#if defined(Q_OS_WINDOWS)
        WIN32_FIND_DATA find_data_;
#else
        QFileInfoList items_;
        int pos_ = 0;
#endif
    };

    explicit FileEnumerator(const QString& root_path);
    ~FileEnumerator();

    const FileInfo& fileInfo() const { return file_info_; }
    bool isAtEnd() const;
    void advance();

private:
    FileInfo file_info_;

#if defined(Q_OS_WINDOWS)
    HANDLE find_handle_ = INVALID_HANDLE_VALUE;
#endif

    Q_DISABLE_COPY(FileEnumerator)
};

}  // namespace base

#endif // BASE_FILES_FILE_ENUMERATOR_H
