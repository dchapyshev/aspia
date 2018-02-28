//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_manager_helpers.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__FILE_MANAGER_HELPERS_H
#define _ASPIA_UI__FILE_TRANSFER__FILE_MANAGER_HELPERS_H

#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>

#include <experimental/filesystem>

namespace aspia {

HICON GetDriveIcon(proto::file_transfer::DriveList::Item::Type drive_type);

HICON GetComputerIcon();

HICON GetDirectoryIcon();

HICON GetFileIcon(const std::experimental::filesystem::path& file_name);

CString GetDriveDisplayName(const proto::file_transfer::DriveList::Item& item);

CString GetDriveDescription(proto::file_transfer::DriveList::Item::Type type);

CString GetFileTypeString(const std::experimental::filesystem::path& file_name);

std::wstring SizeToString(uint64_t size);

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__FILE_MANAGER_HELPERS_H
