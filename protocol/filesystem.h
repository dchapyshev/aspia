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

std::unique_ptr<proto::DriveList> CreateDriveList();

std::unique_ptr<proto::DirectoryList> CreateDirectoryList(
    const std::experimental::filesystem::path& path);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
