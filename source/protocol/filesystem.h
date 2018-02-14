//
// PROJECT:         Aspia
// FILE:            protocol/filesystem.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include <experimental/filesystem>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileSystemRequest
{
public:
    static proto::file_transfer::Status GetDriveList(proto::file_transfer::DriveList* drive_list);

    static proto::file_transfer::Status GetFileList(
        const std::experimental::filesystem::path& path,
        proto::file_transfer::FileList* file_list);

    static proto::file_transfer::Status CreateDirectory(
        const std::experimental::filesystem::path& path);

    static proto::file_transfer::Status GetDirectorySize(
        const std::experimental::filesystem::path& path, uint64_t& size);

    static proto::file_transfer::Status Rename(
        const std::experimental::filesystem::path& old_name,
        const std::experimental::filesystem::path& new_name);

    static proto::file_transfer::Status Remove(
        const std::experimental::filesystem::path& request);

private:
    DISALLOW_COPY_AND_ASSIGN(FileSystemRequest);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
