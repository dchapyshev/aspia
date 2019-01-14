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

#include "client/file_remove_task.h"

namespace client {

FileRemoveTask::FileRemoveTask(const QString& path, bool is_directory)
    : path_(path),
      is_directory_(is_directory)
{
    // Nothing
}

FileRemoveTask::FileRemoveTask(const FileRemoveTask& other) = default;
FileRemoveTask& FileRemoveTask::operator=(const FileRemoveTask& other) = default;

FileRemoveTask::FileRemoveTask(FileRemoveTask&& other) noexcept
    : path_(std::move(other.path_)),
      is_directory_(other.is_directory_)
{
    // Nothing
}

FileRemoveTask& FileRemoveTask::operator=(FileRemoveTask&& other) noexcept
{
    path_ = std::move(other.path_);
    is_directory_ = other.is_directory_;
    return *this;
}

} // namespace client
