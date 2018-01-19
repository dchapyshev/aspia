//
// PROJECT:         Aspia
// FILE:            client/file_task.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TASK_H
#define _ASPIA_CLIENT__FILE_TASK_H

#include "base/macros.h"

#include <experimental/filesystem>
#include <deque>

namespace aspia {

class FileTask
{
public:
    FileTask() = default;

    FileTask(const std::experimental::filesystem::path& source_object_path,
             const std::experimental::filesystem::path& target_object_path,
             uint64_t size,
             bool is_directory);

    FileTask(FileTask&& other) noexcept;
    FileTask& operator=(FileTask&& other) noexcept;

    ~FileTask() = default;

    const std::experimental::filesystem::path& SourcePath() const { return source_object_path_; }
    const std::experimental::filesystem::path& TargetPath() const { return target_object_path_; }

    uint64_t Size() const { return size_; }
    bool IsDirectory() const { return is_directory_; }

private:
    std::experimental::filesystem::path source_object_path_;
    std::experimental::filesystem::path target_object_path_;
    uint64_t size_ = 0;
    bool is_directory_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileTask);
};

using FileTaskQueue = std::deque<FileTask>;

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TASK_H
