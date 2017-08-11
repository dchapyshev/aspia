//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_task.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TASK_H
#define _ASPIA_CLIENT__FILE_TASK_H

#include "base/files/file_path.h"

namespace aspia {

class FileTask
{
public:
    FileTask(const FilePath& source_object_path,
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

    const FilePath& SourcePath() const
    {
        return source_object_path_;
    }

    const FilePath& TargetPath() const
    {
        return target_object_path_;
    }

    uint64_t Size() const
    {
        return size_;
    }

    bool IsDirectory() const
    {
        return is_directory_;
    }

private:
    const FilePath source_object_path_;
    const FilePath target_object_path_;
    const uint64_t size_;
    const bool is_directory_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TASK_H
