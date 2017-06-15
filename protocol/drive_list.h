//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/drive_list.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__DRIVE_LIST_H
#define _ASPIA_PROTOCOL__DRIVE_LIST_H

#include "proto/file_transfer_session.pb.h"

#include <memory>

namespace aspia {

std::unique_ptr<proto::DriveList> CreateDriveList();

} // namespace aspia

#endif // _ASPIA_PROTOCOL__DRIVE_LIST_H
