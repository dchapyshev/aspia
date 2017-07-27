//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/filesystem.h"
#include "base/files/drive_enumerator.h"
#include "base/files/file_helpers.h"
#include "base/files/base_paths.h"
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

proto::RequestStatus ExecuteFileListRequest(const FilePath& path,
                                            proto::FileList* file_list)
{
    DCHECK(file_list);

    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    std::error_code code;

    if (!std::experimental::filesystem::exists(path, code))
        return proto::REQUEST_STATUS_PATH_NOT_FOUND;

    for (auto& entry : std::experimental::filesystem::directory_iterator(path, code))
    {
        proto::FileList::Item* item = file_list->add_item();

        item->set_name(entry.path().filename().u8string());

        std::experimental::filesystem::file_time_type time =
            std::experimental::filesystem::last_write_time(entry.path(), code);

        item->set_modification_time(decltype(time)::clock::to_time_t(time));

        item->set_is_directory(std::experimental::filesystem::is_directory(entry.status()));

        if (!item->is_directory())
        {
            uintmax_t size = std::experimental::filesystem::file_size(entry.path(), code);
            if (size != static_cast<uintmax_t>(-1))
                item->set_size(size);
        }
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
        if (std::experimental::filesystem::exists(path, code))
            return proto::REQUEST_STATUS_PATH_ALREADY_EXISTS;

        return proto::REQUEST_STATUS_ACCESS_DENIED;
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteDirectorySizeRequest(const FilePath& path,
                                                 uint64_t& size)
{
    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    size = 0;

    for (const auto& entry : std::experimental::filesystem::recursive_directory_iterator())
    {
        if (!std::experimental::filesystem::is_directory(entry.status()))
        {
            std::error_code code;

            uintmax_t file_size =
                std::experimental::filesystem::file_size(entry.path(), code);

            if (file_size != static_cast<uintmax_t>(-1))
                size += file_size;
        }
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteRenameRequest(const FilePath& old_name,
                                          const FilePath& new_name)
{
    if (!IsValidPathName(old_name) || !IsValidPathName(new_name))
    {
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;
    }

    if (old_name != new_name)
    {
        std::error_code code;

        if (std::experimental::filesystem::exists(new_name, code))
            return proto::REQUEST_STATUS_PATH_ALREADY_EXISTS;

        std::experimental::filesystem::rename(old_name, new_name, code);
    }

    return proto::REQUEST_STATUS_SUCCESS;
}

proto::RequestStatus ExecuteRemoveRequest(const FilePath& path)
{
    if (!IsValidPathName(path))
        return proto::REQUEST_STATUS_INVALID_PATH_NAME;

    std::error_code code;

    if (!std::experimental::filesystem::exists(path, code))
        return proto::REQUEST_STATUS_PATH_NOT_FOUND;

    if (!std::experimental::filesystem::remove(path, code))
        return proto::REQUEST_STATUS_ACCESS_DENIED;

    return proto::REQUEST_STATUS_SUCCESS;
}

} // namespace aspia
