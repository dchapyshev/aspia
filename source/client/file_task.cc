//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_task.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_task.h"

namespace aspia {

FileTask::FileTask(const FilePath& source_object_path,
         const FilePath& target_object_path,
         uint64_t size,
         bool is_directory)
    : source_object_path_(source_object_path),
      target_object_path_(target_object_path),
      size_(size),
      is_directory_(is_directory)
{
    // Nothing
}

FileTask::FileTask(FileTask&& other) noexcept
{
    source_object_path_ = std::move(other.source_object_path_);
    target_object_path_ = std::move(other.target_object_path_);

    size_ = other.size_;
    is_directory_ = other.is_directory_;

    other.size_ = 0;
    other.is_directory_ = false;
}

FileTask& FileTask::operator=(FileTask&& other) noexcept
{
    source_object_path_ = std::move(other.source_object_path_);
    target_object_path_ = std::move(other.target_object_path_);

    size_ = other.size_;
    is_directory_ = other.is_directory_;

    other.size_ = 0;
    other.is_directory_ = false;

    return *this;
}

} // namespace aspia
