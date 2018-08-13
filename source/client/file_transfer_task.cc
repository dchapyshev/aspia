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

#include "client/file_transfer_task.h"

namespace aspia {

FileTransferTask::FileTransferTask(const QString& source_path,
                                   const QString& target_path,
                                   bool is_directory,
                                   int64_t size)
    : source_path_(source_path),
      target_path_(target_path),
      is_directory_(is_directory),
      size_(size)
{
    // Nothing
}

FileTransferTask::FileTransferTask(FileTransferTask&& other) noexcept
    : source_path_(std::move(other.source_path_)),
      target_path_(std::move(other.target_path_)),
      is_directory_(other.is_directory_),
      size_(other.size_)
{
    // Nothing
}

FileTransferTask& FileTransferTask::operator=(FileTransferTask&& other) noexcept
{
    source_path_ = std::move(other.source_path_);
    target_path_ = std::move(other.target_path_);
    is_directory_ = other.is_directory_;
    size_ = other.size_;
    return *this;
}

} // namespace aspia
