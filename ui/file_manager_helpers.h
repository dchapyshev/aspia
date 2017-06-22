//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_helpers.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_HELPERS_H
#define _ASPIA_UI__FILE_MANAGER_HELPERS_H

#include "proto/file_transfer_session.pb.h"

#include <string>

namespace aspia {

std::wstring TimeToString(time_t time);

HICON GetDriveIcon(proto::DriveListItem::Type drive_type);

HICON GetComputerIcon();

HICON GetDirectoryIcon();

HICON GetFileIcon(const std::wstring& file_name);

std::wstring GetDriveDisplayName(const proto::DriveListItem& item);

std::wstring GetDriveDescription(proto::DriveListItem::Type type);

std::wstring GetDirectoryTypeString(const std::wstring& dir_name);

std::wstring GetFileTypeString(const std::wstring& file_name);

std::wstring SizeToString(uint64_t size);

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_HELPERS_H
