//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "proto/file_transfer_session.pb.h"

#include <filesystem>
#include <memory>

namespace aspia {

std::unique_ptr<proto::DriveList> ExecuteDriveListRequest(
    const proto::DriveListRequest& drive_list_request);

std::unique_ptr<proto::DirectoryList> ExecuteDirectoryListRequest(
    const proto::DirectoryListRequest& directory_list_request);

proto::Status ExecuteCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& create_directory_request);

proto::Status ExecuteRenameRequest(const proto::RenameRequest& rename_request);

proto::Status ExecuteRemoveRequest(const proto::RemoveRequest& remove_request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
