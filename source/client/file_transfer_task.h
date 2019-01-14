//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__FILE_TRANSFER_TASK_H
#define CLIENT__FILE_TRANSFER_TASK_H

#include <QString>

namespace client {

class FileTransferTask
{
public:
    FileTransferTask(const QString& source_path,
                     const QString& target_path,
                     bool is_directory,
                     int64_t size);

    FileTransferTask(const FileTransferTask& other) = default;
    FileTransferTask& operator=(const FileTransferTask& other) = default;

    FileTransferTask(FileTransferTask&& other) noexcept;

    FileTransferTask& operator=(FileTransferTask&& other) noexcept;

    ~FileTransferTask() = default;

    const QString& sourcePath() const { return source_path_; }
    const QString& targetPath() const { return target_path_; }
    bool isDirectory() const { return is_directory_; }
    int64_t size() const { return size_; }

    bool overwrite() const { return overwrite_; }
    void setOverwrite(bool value) { overwrite_ = value; }

private:
    QString source_path_;
    QString target_path_;
    bool is_directory_;
    bool overwrite_ = false;
    int64_t size_;
};

} // namespace client

#endif // CLIENT__FILE_TRANSFER_TASK_H
