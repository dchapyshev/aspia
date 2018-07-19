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

#ifndef ASPIA_CLIENT__FILE_REMOVE_TASK_H_
#define ASPIA_CLIENT__FILE_REMOVE_TASK_H_

#include <QString>

namespace aspia {

class FileRemoveTask
{
public:
    FileRemoveTask(const QString& path, bool is_directory);

    FileRemoveTask(const FileRemoveTask& other);
    FileRemoveTask& operator=(const FileRemoveTask& other);

    FileRemoveTask(FileRemoveTask&& other) noexcept;
    FileRemoveTask& operator=(FileRemoveTask&& other) noexcept;

    ~FileRemoveTask() = default;

    const QString& path() const { return path_; }
    bool isDirectory() const { return is_directory_; }

private:
    QString path_;
    bool is_directory_;
};

} // namespace aspia

#endif // ASPIA_CLIENT__FILE_REMOVE_TASK_H_
