//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILESYSTEM_H
#define _ASPIA_PROTOCOL__FILESYSTEM_H

#include "base/files/file_path.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

proto::RequestStatus ExecuteDriveListRequest(proto::DriveList* drive_list);

proto::RequestStatus ExecuteFileListRequest(const FilePath& path,
                                            proto::FileList* file_list);

proto::RequestStatus ExecuteCreateDirectoryRequest(const FilePath& path);

proto::RequestStatus ExecuteDirectorySizeRequest(const FilePath& path,
                                                 uint64_t& size);

proto::RequestStatus ExecuteRenameRequest(const FilePath& old_name,
                                          const FilePath& new_name);

proto::RequestStatus ExecuteRemoveRequest(const FilePath& request);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILESYSTEM_H
