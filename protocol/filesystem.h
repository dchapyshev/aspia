//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "proto/file_transfer_session.pb.h"

#include <memory>

namespace aspia {

std::unique_ptr<proto::RequestStatus> ExecuteDriveListRequest(
    const proto::DriveListRequest& request, std::unique_ptr<proto::DriveList>& reply);

std::unique_ptr<proto::RequestStatus> ExecuteDirectoryListRequest(
    const proto::DirectoryListRequest& request,
    std::unique_ptr<proto::DirectoryList>& reply);

std::unique_ptr<proto::RequestStatus> ExecuteCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request);

std::unique_ptr<proto::RequestStatus> ExecuteRenameRequest(
    const proto::RenameRequest& request);

std::unique_ptr<proto::RequestStatus> ExecuteRemoveRequest(
    const proto::RemoveRequest& request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
