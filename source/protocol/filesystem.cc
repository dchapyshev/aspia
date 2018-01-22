//
// PROJECT:         Aspia
// FILE:            protocol/filesystem.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/filesystem.h"
#include "base/files/logical_drive_enumerator.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_helpers.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

proto::file_transfer::Status ExecuteDriveListRequest(proto::file_transfer::DriveList* drive_list)
{
    DCHECK(drive_list);

    LogicalDriveEnumerator enumerator;

    for (;;)
    {
        std::experimental::filesystem::path path = enumerator.Next();

        if (path.empty())
            break;

        proto::file_transfer::DriveList::Item* item = drive_list->add_item();

        item->set_path(UTF8fromUNICODE(path));

        LogicalDriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(drive_info.VolumeName()));
        item->set_total_space(drive_info.TotalSpace());
        item->set_free_space(drive_info.FreeSpace());

        switch (drive_info.Type())
        {
            case LogicalDriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_CDROM);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_REMOVABLE);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_FIXED);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_RAM);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_REMOTE);
                break;

            default:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_UNKNOWN);
                break;
        }
    }

    std::experimental::filesystem::path path;

    if (GetBasePath(BasePathKey::DIR_USER_HOME, path))
    {
        proto::file_transfer::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(path.u8string());
    }

    if (GetBasePath(BasePathKey::DIR_USER_DESKTOP, path))
    {
        proto::file_transfer::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(path.u8string());
    }

    if (!drive_list->item_size())
        return proto::file_transfer::STATUS_NO_DRIVES_FOUND;

    return proto::file_transfer::STATUS_SUCCESS;
}

proto::file_transfer::Status ExecuteFileListRequest(
    const std::experimental::filesystem::path& path,
    proto::file_transfer::FileList* file_list)
{
    DCHECK(file_list);

    if (!IsValidPathName(path))
        return proto::file_transfer::STATUS_INVALID_PATH_NAME;

    if (!DirectoryExists(path))
        return proto::file_transfer::STATUS_PATH_NOT_FOUND;

    FileEnumerator enumerator(path, false, FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

    for (std::experimental::filesystem::path current = enumerator.Next();
         !current.empty();
         current = enumerator.Next())
    {
        FileEnumerator::FileInfo info = enumerator.GetInfo();
        proto::file_transfer::FileList::Item* item = file_list->add_item();

        item->set_name(info.GetName().u8string());
        item->set_modification_time(info.GetLastModifiedTime());
        item->set_is_directory(info.IsDirectory());
        item->set_size(info.GetSize());
    }

    return proto::file_transfer::STATUS_SUCCESS;
}

proto::file_transfer::Status ExecuteCreateDirectoryRequest(
    const std::experimental::filesystem::path& path)
{
    if (!IsValidPathName(path))
        return proto::file_transfer::STATUS_INVALID_PATH_NAME;

    if (!CreateDirectory(path, nullptr))
    {
        if (PathExists(path))
            return proto::file_transfer::STATUS_PATH_ALREADY_EXISTS;

        return proto::file_transfer::STATUS_ACCESS_DENIED;
    }

    return proto::file_transfer::STATUS_SUCCESS;
}

proto::file_transfer::Status ExecuteDirectorySizeRequest(
    const std::experimental::filesystem::path& path, uint64_t& size)
{
    if (!IsValidPathName(path))
        return proto::file_transfer::STATUS_INVALID_PATH_NAME;

    size = ComputeDirectorySize(path);

    return proto::file_transfer::STATUS_SUCCESS;
}

proto::file_transfer::Status ExecuteRenameRequest(
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name)
{
    if (!IsValidPathName(old_name) || !IsValidPathName(new_name))
    {
        return proto::file_transfer::STATUS_INVALID_PATH_NAME;
    }

    if (old_name != new_name)
    {
        if (PathExists(new_name))
            return proto::file_transfer::STATUS_PATH_ALREADY_EXISTS;

        if (!ReplaceFile(old_name, new_name, nullptr))
            return proto::file_transfer::STATUS_ACCESS_DENIED;
    }

    return proto::file_transfer::STATUS_SUCCESS;
}

proto::file_transfer::Status ExecuteRemoveRequest(const std::experimental::filesystem::path& path)
{
    if (!IsValidPathName(path))
        return proto::file_transfer::STATUS_INVALID_PATH_NAME;

    if (!PathExists(path))
        return proto::file_transfer::STATUS_PATH_NOT_FOUND;

    if (!DeleteFile(path))
        return proto::file_transfer::STATUS_ACCESS_DENIED;

    return proto::file_transfer::STATUS_SUCCESS;
}

} // namespace aspia
