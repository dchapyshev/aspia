//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "base/files/file_path.h"
#include "proto/file_transfer_session.pb.h"

#include <memory>

namespace aspia {

proto::Status ExecuteDriveListRequest(proto::DriveList* drive_list);

proto::Status ExecuteFileListRequest(const FilePath& path, proto::FileList* file_list);

proto::Status ExecuteCreateDirectoryRequest(const FilePath& path);

proto::Status ExecuteRenameRequest(const FilePath& old_name, const FilePath& new_name);

proto::Status ExecuteRemoveRequest(const FilePath& request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
