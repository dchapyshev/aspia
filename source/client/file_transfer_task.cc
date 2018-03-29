//
// PROJECT:         Aspia
// FILE:            client/file_transfer_task.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_task.h"

namespace aspia {

FileTransferTask::FileTransferTask(const QString& source_path,
                                   const QString& target_path,
                                   bool is_directory)
    : source_path_(source_path),
      target_path_(target_path),
      is_directory_(is_directory)
{
    // Nothing
}

FileTransferTask::FileTransferTask(const FileTransferTask& other) = default;
FileTransferTask& FileTransferTask::operator=(const FileTransferTask& other) = default;

FileTransferTask::FileTransferTask(FileTransferTask&& other) noexcept
    : source_path_(std::move(other.source_path_)),
      target_path_(std::move(other.target_path_)),
      is_directory_(other.is_directory_)
{
    // Nothing
}

FileTransferTask& FileTransferTask::operator=(FileTransferTask&& other) noexcept
{
    source_path_ = std::move(other.source_path_);
    target_path_ = std::move(other.target_path_);
    is_directory_ = other.is_directory_;
    return *this;
}

FileTransferTask::~FileTransferTask() = default;

const QString& FileTransferTask::sourcePath() const
{
    return source_path_;
}

const QString& FileTransferTask::targetPath() const
{
    return target_path_;
}

bool FileTransferTask::isDirectory() const
{
    return is_directory_;
}

} // namespace aspia
