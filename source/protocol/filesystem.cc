//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/filesystem.h"
#include "base/files/drive_enumerator.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_helpers.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

proto::RequestStatus ExecuteDriveListRequest(proto::DriveList* drive_list)
{
    DCHECK(drive_list);

    DriveEnumerator enumerator;

    for (;;)
    {
        FilePath path = enumerator.Next();

        if (path.empty())
            break;

        proto::DriveList::Item* item = drive_list->add_item();

        item->set_path(UTF8fromUNICODE(path));

        DriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(drive_info.VolumeName()));
        item->set_total_space(drive_info.TotalSpace());
        item->set_free_space(drive_info.FreeSpace());

        switch (drive_info.Type())
        {
            case DriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_type(proto::DriveList::Item::CDROM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_type(proto::DriveList::Item::REMOVABLE);
                break;

            case DriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_type(proto::DriveList::Item::FIXED);
                break;

            case DriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_type(proto::DriveList::Item::RAM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_type(proto::DriveList::Item::REMOTE);
                break;

            default:
                item->set_type(proto::DriveList::Item::UNKNOWN);
                break;
        }
    }

    FilePath path;

    if (GetBasePath(BasePathKey::DIR_USER_HOME, path))
    {
        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::HOME_FOLDER);
        item->set_path(path.u8string());
    }

    if (GetBasePath(BasePathKey::DIR_USER_DESKTOP, path))
    {
        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::DESKTOP_FOLDER);
        item->set_path(path.u8string());
    }

    if (!drive_list->item_size())
        return proto::REQUEST_STATUS_NO_DRIVES_FOUND;

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteFileListRequest(const FilePath& path, proto::FileList* file_list)
{
    DCHECK(file_list);

    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    std::error_code code;

    if (!DirectoryExists(path))
        return proto::REQUEST_STATUS_PATH_NOT_FOUND;

    FileEnumerator enumerator(path, false, FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

    for (FilePath current = enumerator.Next(); !current.empty(); current = enumerator.Next())
    {
        FileEnumerator::FileInfo info = enumerator.GetInfo();
        proto::FileList::Item* item = file_list->add_item();

        item->set_name(info.GetName().u8string());
        item->set_modification_time(info.GetLastModifiedTime());
        item->set_is_directory(info.IsDirectory());
        item->set_size(info.GetSize());
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteCreateDirectoryRequest(const FilePath& path)
{
    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    std::error_code code;

    if (!std::experimental::filesystem::create_directory(path, code))
    {
        if (PathExists(path))
            return proto::REQUEST_STATUS_PATH_ALREADY_EXISTS;

        return proto::REQUEST_STATUS_ACCESS_DENIED;
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteDirectorySizeRequest(const FilePath& path, uint64_t& size)
{
    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    size = ComputeDirectorySize(path);

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteRenameRequest(const FilePath& old_name, const FilePath& new_name)
{
    if (!IsValidPathName(old_name) || !IsValidPathName(new_name))
    {
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;
    }

    if (old_name != new_name)
    {
        if (PathExists(new_name))
            return proto::REQUEST_STATUS_PATH_ALREADY_EXISTS;

        std::error_code code;
        std::experimental::filesystem::rename(old_name, new_name, code);
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteRemoveRequest(const FilePath& path)
{
    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    if (!PathExists(path))
        return proto::REQUEST_STATUS_PATH_NOT_FOUND;

    if (!DeleteFile(path))
        return proto::REQUEST_STATUS_ACCESS_DENIED;

    return proto::REQUEST_STATUS_SUCCESS;
}

} // namespace aspia
