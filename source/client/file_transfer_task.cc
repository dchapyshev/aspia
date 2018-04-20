//
// PROJECT:         Aspia
// FILE:            client/file_transfer_task.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_task.h"

namespace aspia {

FileTransferTask::FileTransferTask(const QString& source_path,
                                   const QString& target_path,
                                   bool is_directory,
                                   qint64 size)
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
