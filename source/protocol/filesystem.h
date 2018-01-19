//
// PROJECT:         Aspia
// FILE:            protocol/filesystem.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "proto/file_transfer_session.pb.h"

#include <experimental/filesystem>

namespace aspia {

proto::file_transfer::Status ExecuteDriveListRequest(proto::file_transfer::DriveList* drive_list);

proto::file_transfer::Status ExecuteFileListRequest(
    const std::experimental::filesystem::path& path,
    proto::file_transfer::FileList* file_list);

proto::file_transfer::Status ExecuteCreateDirectoryRequest(
    const std::experimental::filesystem::path& path);

proto::file_transfer::Status ExecuteDirectorySizeRequest(
    const std::experimental::filesystem::path& path, uint64_t& size);

proto::file_transfer::Status ExecuteRenameRequest(
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name);

proto::file_transfer::Status ExecuteRemoveRequest(
    const std::experimental::filesystem::path& request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
