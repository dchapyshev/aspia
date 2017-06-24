//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/filesystem.h"
#include "base/files/drive_enumerator.h"
#include "base/strings/unicode.h"
#include "base/path.h"
#include "base/logging.h"

namespace aspia {

namespace fs = std::experimental::filesystem;

std::unique_ptr<proto::DriveList> ExecuteDriveListRequest(
    const proto::DriveListRequest& drive_list_request)
{
    UNREF(drive_list_request);

    std::unique_ptr<proto::DriveList> drive_list(new proto::DriveList());

    DriveEnumerator enumerator;

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DriveListItem* item = drive_list->add_item();

        item->set_path(UTF8fromUNICODE(path));

        DriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(drive_info.VolumeName()));
        item->set_total_space(drive_info.TotalSpace());
        item->set_free_space(drive_info.FreeSpace());

        switch (drive_info.Type())
        {
            case DriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_type(proto::DriveListItem::CDROM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_type(proto::DriveListItem::REMOVABLE);
                break;

            case DriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_type(proto::DriveListItem::FIXED);
                break;

            case DriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_type(proto::DriveListItem::RAM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_type(proto::DriveListItem::REMOTE);
                break;

            default:
                item->set_type(proto::DriveListItem::UNKNOWN);
                break;
        }
    }

    std::wstring path;

    if (GetPathW(PathKey::DIR_USER_HOME, path))
    {
        proto::DriveListItem* item = drive_list->add_item();

        item->set_type(proto::DriveListItem::HOME_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    if (GetPathW(PathKey::DIR_USER_DESKTOP, path))
    {
        proto::DriveListItem* item = drive_list->add_item();

        item->set_type(proto::DriveListItem::DESKTOP_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    return drive_list;
}

proto::Status ExecuteDirectoryListRequest(
    const proto::DirectoryListRequest& request,
    std::unique_ptr<proto::DirectoryList>& reply)
{
    fs::path path = fs::u8path(request.path());

    if (!request.item().empty())
    {
        if (request.item() == "..")
        {
            if (path.has_parent_path() && path.parent_path() != path.root_name())
            {
                path = path.parent_path();
            }
        }
        else
        {
            path.append(fs::u8path(request.item()));
        }
    }

    std::error_code code;

    if (!fs::exists(path, code))
        return proto::Status::STATUS_PATH_NOT_FOUND;

    reply.reset(new proto::DirectoryList());
    reply->set_path(path.u8string());

    if (path.has_parent_path() && path.parent_path() != path.root_name())
    {
        reply->set_has_parent(true);
    }

    for (auto& entry : fs::directory_iterator(path, code))
    {
        proto::DirectoryListItem* item = reply->add_item();

        item->set_name(entry.path().filename().u8string());

        fs::file_time_type time = fs::last_write_time(entry.path(), code);
        item->set_modified(decltype(time)::clock::to_time_t(time));

        if (entry.status().type() == fs::file_type::directory)
        {
            item->set_type(proto::DirectoryListItem::DIRECTORY);
        }
        else
        {
            item->set_type(proto::DirectoryListItem::FILE);

            uintmax_t size = fs::file_size(entry.path(), code);
            if (size != -1)
                item->set_size(size);
        }
    }

    return proto::Status::STATUS_SUCCESS;
}

proto::Status ExecuteCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& create_directory_request)
{
    fs::path path = fs::u8path(create_directory_request.path());

    path.append(fs::u8path(create_directory_request.name()));

    std::error_code code;

    if (!fs::create_directory(path, code))
    {
        if (fs::exists(path, code))
        {
            if (fs::is_directory(path, code))
                return proto::Status::STATUS_PATH_ALREADY_EXISTS;
            else
                return proto::Status::STATUS_FILE_ALREADY_EXISTS;
        }

        return proto::Status::STATUS_ACCESS_DENIED;
    }

    return proto::Status::STATUS_SUCCESS;
}

proto::Status ExecuteRenameRequest(const proto::RenameRequest& rename_request)
{
    fs::path old_path = fs::u8path(rename_request.path());
    old_path.append(fs::u8path(rename_request.old_item_name()));

    fs::path new_path = fs::u8path(rename_request.path());
    new_path.append(fs::u8path(rename_request.new_item_name()));

    if (old_path == new_path)
        return proto::Status::STATUS_SUCCESS;

    std::error_code code;

    if (fs::exists(new_path, code))
    {
        if (fs::is_directory(new_path, code))
            return proto::Status::STATUS_PATH_ALREADY_EXISTS;

        return proto::Status::STATUS_FILE_ALREADY_EXISTS;
    }

    fs::rename(old_path, new_path, code);

    return proto::Status::STATUS_SUCCESS;
}

proto::Status ExecuteRemoveRequest(const proto::RemoveRequest& remove_request)
{
    fs::path path = fs::u8path(remove_request.path());
    path.append(fs::u8path(remove_request.item_name()));

    std::error_code code;

    if (!fs::exists(path, code))
        return proto::Status::STATUS_PATH_NOT_FOUND;

    if (fs::remove_all(path, code) == static_cast<std::uintmax_t>(-1))
        return proto::Status::STATUS_ACCESS_DENIED;

    return proto::Status::STATUS_SUCCESS;
}

} // namespace aspia
