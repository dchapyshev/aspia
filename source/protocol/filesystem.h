//
// PROJECT:         Aspia
// FILE:            protocol/filesystem.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "base/files/file_path.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

proto::file_transfer::Status ExecuteDriveListRequest(proto::file_transfer::DriveList* drive_list);
proto::file_transfer::Status ExecuteFileListRequest(const FilePath& path,
                                                    proto::file_transfer::FileList* file_list);
proto::file_transfer::Status ExecuteCreateDirectoryRequest(const FilePath& path);
proto::file_transfer::Status ExecuteDirectorySizeRequest(const FilePath& path, uint64_t& size);
proto::file_transfer::Status ExecuteRenameRequest(const FilePath& old_name,
                                                  const FilePath& new_name);
proto::file_transfer::Status ExecuteRemoveRequest(const FilePath& request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
