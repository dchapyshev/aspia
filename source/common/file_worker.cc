//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "common/file_worker.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_enumerator.h"
#include "common/file_platform_util.h"

#if defined(OS_WIN)
#include "base/win/drive_enumerator.h"
#endif // defined(OS_WIN)

namespace common {

//--------------------------------------------------------------------------------------------------
FileWorker::FileWorker()
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileWorker::~FileWorker()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRequest(base::local_shared_ptr<FileTask> task)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();
    doRequest(task->request(), reply.get());
    task->setReply(std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRequest(const proto::FileRequest& request, proto::FileReply* reply)
{
#if defined(OS_WIN)
    // We send a notification to the system that it is used to prevent the screen saver, going into
    // hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
#endif // defined(OS_WIN)

    if (request.has_drive_list_request())
    {
        doDriveListRequest(reply);
    }
    else if (request.has_file_list_request())
    {
        doFileListRequest(request.file_list_request(), reply);
    }
    else if (request.has_create_directory_request())
    {
        doCreateDirectoryRequest(request.create_directory_request(), reply);
    }
    else if (request.has_rename_request())
    {
        doRenameRequest(request.rename_request(), reply);
    }
    else if (request.has_remove_request())
    {
        doRemoveRequest(request.remove_request(), reply);
    }
    else if (request.has_download_request())
    {
        doDownloadRequest(request.download_request(), reply);
    }
    else if (request.has_upload_request())
    {
        doUploadRequest(request.upload_request(), reply);
    }
    else if (request.has_packet_request())
    {
        doPacketRequest(request.packet_request(), reply);
    }
    else if (request.has_packet())
    {
        doPacket(request.packet(), reply);
    }
    else
    {
        reply->set_error_code(proto::FILE_ERROR_INVALID_REQUEST);
    }
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doDriveListRequest(proto::FileReply* reply)
{
    proto::DriveList* drive_list = reply->mutable_drive_list();

#if defined(OS_WIN)
    for (base::win::DriveEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::DriveList::Item* item = drive_list->add_item();

        const base::win::DriveEnumerator::DriveInfo& drive_info = enumerator.driveInfo();
        base::win::DriveEnumerator::DriveInfo::Type drive_type = drive_info.type();

        switch (drive_type)
        {
            case base::win::DriveEnumerator::DriveInfo::Type::FIXED:
                item->set_type(proto::DriveList::Item::TYPE_FIXED);
                break;

            case base::win::DriveEnumerator::DriveInfo::Type::CDROM:
                item->set_type(proto::DriveList::Item::TYPE_CDROM);
                break;

            case base::win::DriveEnumerator::DriveInfo::Type::REMOVABLE:
                item->set_type(proto::DriveList::Item::TYPE_REMOVABLE);
                break;

            case base::win::DriveEnumerator::DriveInfo::Type::RAM:
                item->set_type(proto::DriveList::Item::TYPE_RAM);
                break;

            case base::win::DriveEnumerator::DriveInfo::Type::REMOTE:
                item->set_type(proto::DriveList::Item::TYPE_REMOTE);
                break;

            default:
                break;
        }

        item->set_path(base::utf8FromFilePath(drive_info.path()));
    }
#elif (OS_POSIX)
    proto::DriveList::Item* root_directory = drive_list->add_item();
    root_directory->set_type(proto::DriveList::Item::TYPE_ROOT_DIRECTORY);
    root_directory->set_path("/");
#endif

    std::filesystem::path desktop_path;
    if (base::BasePaths::userDesktop(&desktop_path))
    {
        LOG(LS_INFO) << "User desktop path: " << desktop_path.u8string();

        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(base::utf8FromFilePath(desktop_path));
    }

    std::filesystem::path home_path;
    if (base::BasePaths::userHome(&home_path))
    {
        LOG(LS_INFO) << "Home path: " << home_path.u8string();

        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(base::utf8FromFilePath(home_path));
    }

    if (drive_list->item_size() == 0)
        reply->set_error_code(proto::FILE_ERROR_NO_DRIVES_FOUND);
    else
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doFileListRequest(const proto::FileListRequest& request, proto::FileReply* reply)
{
    std::filesystem::path path = base::filePathFromUtf8(request.path());

    std::error_code ignored_code;
    std::filesystem::file_status status = std::filesystem::status(path, ignored_code);

    if (!std::filesystem::exists(status))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return;
    }

    if (!std::filesystem::is_directory(status))
    {
        reply->set_error_code(proto::FILE_ERROR_INVALID_PATH_NAME);
        return;
    }

    proto::FileList* file_list = reply->mutable_file_list();
    FileEnumerator enumerator(path);

    while (!enumerator.isAtEnd())
    {
        const FileEnumerator::FileInfo& file_info = enumerator.fileInfo();

        proto::FileList::Item* item = file_list->add_item();
        item->set_name(file_info.u8name());
        item->set_size(static_cast<uint64_t>(file_info.size()));
        item->set_modification_time(file_info.lastWriteTime());
        item->set_is_directory(file_info.isDirectory());

        enumerator.advance();
    }

    reply->set_error_code(enumerator.errorCode());
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request, proto::FileReply* reply)
{
    std::filesystem::path directory_path = base::filePathFromUtf8(request.path());

    std::error_code ignored_code;
    if (std::filesystem::exists(directory_path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return;
    }

    if (!std::filesystem::create_directory(directory_path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRenameRequest(const proto::RenameRequest& request, proto::FileReply* reply)
{
    std::filesystem::path old_name = base::filePathFromUtf8(request.old_name());
    std::filesystem::path new_name = base::filePathFromUtf8(request.new_name());

    if (old_name == new_name)
    {
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
        return;
    }

    std::error_code error_code;
    if (!std::filesystem::exists(old_name, error_code))
    {
        if (error_code)
            reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        else
            reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);

        return;
    }

    if (std::filesystem::exists(new_name, error_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return;
    }
    else
    {
        if (error_code)
        {
            reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
            return;
        }
    }

    std::filesystem::rename(old_name, new_name, error_code);
    if (error_code)
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    return;
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRemoveRequest(const proto::RemoveRequest& request, proto::FileReply* reply)
{
    std::filesystem::path path = base::filePathFromUtf8(request.path());

    std::error_code error_code;
    if (!std::filesystem::exists(path, error_code))
    {
        if (error_code)
            reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        else
            reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);

        return;
    }

    std::error_code ignored_code;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
        std::filesystem::perm_options::add,
        ignored_code);

    if (!std::filesystem::remove(path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doDownloadRequest(const proto::DownloadRequest& request, proto::FileReply* reply)
{
    packetizer_ = FilePacketizer::create(base::filePathFromUtf8(request.path()));
    if (!packetizer_)
        reply->set_error_code(proto::FILE_ERROR_FILE_OPEN_ERROR);
    else
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doUploadRequest(const proto::UploadRequest& request, proto::FileReply* reply)
{
    std::filesystem::path file_path = base::filePathFromUtf8(request.path());

    do
    {
        if (!request.overwrite())
        {
            std::error_code ignored_code;
            if (std::filesystem::exists(file_path, ignored_code))
            {
                reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(file_path, request.overwrite());
        if (!depacketizer_)
        {
            reply->set_error_code(proto::FILE_ERROR_FILE_CREATE_ERROR);
            break;
        }

        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    }
    while (false);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doPacketRequest(const proto::FilePacketRequest& request, proto::FileReply* reply)
{
    if (!packetizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::FILE_ERROR_UNKNOWN);
        LOG(LS_ERROR) << "Unexpected file packet request";
    }
    else
    {
        std::unique_ptr<proto::FilePacket> packet = packetizer_->readNextPacket(request);
        if (!packet)
        {
            reply->set_error_code(proto::FILE_ERROR_FILE_READ_ERROR);
            packetizer_.reset();
        }
        else
        {
            if (packet->flags() & proto::FilePacket::LAST_PACKET)
                packetizer_.reset();

            reply->set_error_code(proto::FILE_ERROR_SUCCESS);
            reply->set_allocated_packet(packet.release());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doPacket(const proto::FilePacket& packet, proto::FileReply* reply)
{
    if (!depacketizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::FILE_ERROR_UNKNOWN);
        LOG(LS_ERROR) << "Unexpected file packet";
    }
    else
    {
        if (!depacketizer_->writeNextPacket(packet))
        {
            reply->set_error_code(proto::FILE_ERROR_FILE_WRITE_ERROR);
            depacketizer_.reset();
        }
        else
        {
            reply->set_error_code(proto::FILE_ERROR_SUCCESS);
        }

        if (packet.flags() & proto::FilePacket::LAST_PACKET)
            depacketizer_.reset();
    }
}

} // namespace common
