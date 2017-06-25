//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/filesystem.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/filesystem.h"
#include "base/files/drive_enumerator.h"
#include "base/files/file_helpers.h"
#include "base/strings/unicode.h"
#include "base/path.h"
#include "base/logging.h"

#include <filesystem>

namespace aspia {

namespace fs = std::experimental::filesystem;

static std::unique_ptr<proto::RequestStatus>
CreateRequestStatus(proto::RequestStatus::Type request_type,
                    const std::string& first_path,
                    const std::string& second_path,
                    proto::Status status_code)
{
    std::unique_ptr<proto::RequestStatus> status(new proto::RequestStatus());

    status->set_type(request_type);
    status->set_code(status_code);
    status->set_first_path(first_path);
    status->set_second_path(second_path);

    return status;
}

std::unique_ptr<proto::RequestStatus> ExecuteDriveListRequest(
    const proto::DriveListRequest& request,
    std::unique_ptr<proto::DriveList>& reply)
{
    UNREF(request);

    reply.reset(new proto::DriveList());

    DriveEnumerator enumerator;

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DriveListItem* item = reply->add_item();

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

    fs::path path;

    if (GetBasePath(PathKey::DIR_USER_HOME, path))
    {
        proto::DriveListItem* item = reply->add_item();

        item->set_type(proto::DriveListItem::HOME_FOLDER);
        item->set_path(path.u8string());
    }

    if (GetBasePath(PathKey::DIR_USER_DESKTOP, path))
    {
        proto::DriveListItem* item = reply->add_item();

        item->set_type(proto::DriveListItem::DESKTOP_FOLDER);
        item->set_path(path.u8string());
    }

    proto::Status status_code = proto::Status::STATUS_SUCCESS;

    if (!reply->item_size())
        status_code = proto::Status::STATUS_NO_DRIVES_FOUND;

    return CreateRequestStatus(proto::RequestStatus::DRIVE_LIST,
                               std::string(),
                               std::string(),
                               status_code);
}

std::unique_ptr<proto::RequestStatus> ExecuteDirectoryListRequest(
    const proto::DirectoryListRequest& request,
    std::unique_ptr<proto::DirectoryList>& reply)
{
    if (!IsValidPathName(request.path()))
    {
        return CreateRequestStatus(proto::RequestStatus::DIRECTORY_LIST,
                                   std::string(),
                                   std::string(),
                                   proto::Status::STATUS_INVALID_PATH_NAME);
    }

    if (!request.item().empty() && !IsValidFileName(request.item()))
    {
        return CreateRequestStatus(proto::RequestStatus::DIRECTORY_LIST,
                                   std::string(),
                                   std::string(),
                                   proto::Status::STATUS_INVALID_PATH_NAME);
    }

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
    {
        return CreateRequestStatus(proto::RequestStatus::DIRECTORY_LIST,
                                   path.u8string(),
                                   std::string(),
                                   proto::Status::STATUS_PATH_NOT_FOUND);
    }

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

    return CreateRequestStatus(proto::RequestStatus::DIRECTORY_LIST,
                               path.u8string(),
                               std::string(),
                               proto::Status::STATUS_SUCCESS);
}

std::unique_ptr<proto::RequestStatus> ExecuteCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    if (!IsValidPathName(request.path()) || !IsValidFileName(request.name()))
    {
        return CreateRequestStatus(proto::RequestStatus::CREATE_DIRECTORY,
                                   std::string(),
                                   std::string(),
                                   proto::Status::STATUS_INVALID_PATH_NAME);
    }

    proto::Status status_code = proto::Status::STATUS_SUCCESS;

    fs::path path = fs::u8path(request.path());
    path.append(fs::u8path(request.name()));

    std::error_code code;

    if (!fs::create_directory(path, code))
    {
        if (fs::exists(path, code))
        {
            if (fs::is_directory(path, code))
            {
                status_code = proto::Status::STATUS_PATH_ALREADY_EXISTS;
            }
            else
            {
                status_code = proto::Status::STATUS_FILE_ALREADY_EXISTS;
            }
        }
        else
        {
            status_code = proto::Status::STATUS_ACCESS_DENIED;
        }
    }

    return CreateRequestStatus(proto::RequestStatus::CREATE_DIRECTORY,
                               path.u8string(),
                               std::string(),
                               status_code);
}

std::unique_ptr<proto::RequestStatus> ExecuteRenameRequest(
    const proto::RenameRequest& request)
{
    if (!IsValidPathName(request.path()) ||
        !IsValidFileName(request.old_item_name()) ||
        !IsValidFileName(request.new_item_name()))
    {
        return CreateRequestStatus(proto::RequestStatus::RENAME,
                                   std::string(),
                                   std::string(),
                                   proto::Status::STATUS_INVALID_PATH_NAME);
    }

    proto::Status status_code = proto::Status::STATUS_SUCCESS;

    fs::path old_path = fs::u8path(request.path());
    old_path.append(fs::u8path(request.old_item_name()));

    fs::path new_path = fs::u8path(request.path());
    new_path.append(fs::u8path(request.new_item_name()));

    if (old_path != new_path)
    {
        std::error_code code;

        if (fs::exists(new_path, code))
        {
            if (fs::is_directory(new_path, code))
            {
                status_code = proto::Status::STATUS_PATH_ALREADY_EXISTS;
            }
            else
            {
                status_code = proto::Status::STATUS_FILE_ALREADY_EXISTS;
            }
        }
        else
        {
            fs::rename(old_path, new_path, code);
        }
    }

    return CreateRequestStatus(proto::RequestStatus::RENAME,
                               old_path.u8string(),
                               new_path.u8string(),
                               status_code);
}

std::unique_ptr<proto::RequestStatus> ExecuteRemoveRequest(
    const proto::RemoveRequest& request)
{
    if (!IsValidPathName(request.path()) || !IsValidFileName(request.item_name()))
    {
        return CreateRequestStatus(proto::RequestStatus::REMOVE,
                                   std::string(),
                                   std::string(),
                                   proto::Status::STATUS_INVALID_PATH_NAME);
    }

    proto::Status status_code = proto::Status::STATUS_SUCCESS;

    fs::path path = fs::u8path(request.path());
    path.append(fs::u8path(request.item_name()));

    std::error_code code;

    if (!fs::exists(path, code))
    {
        status_code = proto::Status::STATUS_PATH_NOT_FOUND;
    }
    else
    {
        if (fs::remove_all(path, code) == static_cast<std::uintmax_t>(-1))
        {
            status_code = proto::Status::STATUS_ACCESS_DENIED;
        }
    }

    return CreateRequestStatus(proto::RequestStatus::REMOVE,
                               path.u8string(),
                               std::string(),
                               status_code);
}

} // namespace aspia
