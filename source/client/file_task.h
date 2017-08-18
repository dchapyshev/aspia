//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_task.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TASK_H
#define _ASPIA_CLIENT__FILE_TASK_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include <deque>

namespace aspia {

class FileTask
{
public:
    FileTask() = default;

    FileTask(const FilePath& source_object_path,
             const FilePath& target_object_path,
             uint64_t size,
             bool is_directory);

    FileTask(FileTask&& other) noexcept;
    FileTask& operator=(FileTask&& other) noexcept;

    ~FileTask() = default;

    const FilePath& SourcePath() const { return source_object_path_; }
    const FilePath& TargetPath() const { return target_object_path_; }

    uint64_t Size() const { return size_; }
    bool IsDirectory() const { return is_directory_; }

private:
    FilePath source_object_path_;
    FilePath target_object_path_;
    uint64_t size_ = 0;
    bool is_directory_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileTask);
};

using FileTaskQueue = std::deque<FileTask>;

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TASK_H
