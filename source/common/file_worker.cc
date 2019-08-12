//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"
#include "base/base_paths.h"
#include "base/logging.h"
#include "common/file_platform_util.h"

#if defined(OS_WIN)
#include "base/win/drive_enumerator.h"
#include "common/win/file_enumerator.h"
#endif // defined(OS_WIN)

namespace common {

FileWorker::FileWorker(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

proto::FileReply FileWorker::doRequest(const proto::FileRequest& request)
{
#if defined(OS_WIN)
    // We send a notification to the system that it is used to prevent the screen saver, going into
    // hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
#endif

    if (request.has_drive_list_request())
    {
        return doDriveListRequest();
    }
    else if (request.has_file_list_request())
    {
        return doFileListRequest(request.file_list_request());
    }
    else if (request.has_create_directory_request())
    {
        return doCreateDirectoryRequest(request.create_directory_request());
    }
    else if (request.has_rename_request())
    {
        return doRenameRequest(request.rename_request());
    }
    else if (request.has_remove_request())
    {
        return doRemoveRequest(request.remove_request());
    }
    else if (request.has_download_request())
    {
        return doDownloadRequest(request.download_request());
    }
    else if (request.has_upload_request())
    {
        return doUploadRequest(request.upload_request());
    }
    else if (request.has_packet_request())
    {
        return doPacketRequest(request.packet_request());
    }
    else if (request.has_packet())
    {
        return doPacket(request.packet());
    }
    else
    {
        proto::FileReply reply;
        reply.set_status(proto::FileReply::STATUS_INVALID_REQUEST);
        return reply;
    }
}

void FileWorker::executeRequest(FileRequest* request)
{
    std::unique_ptr<FileRequest> request_deleter(request);
    request->sendReply(doRequest(request->request()));
}

proto::FileReply FileWorker::doDriveListRequest()
{
    proto::FileReply reply;

    for (base::win::DriveEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        const base::win::DriveEnumerator::DriveInfo& drive_info = enumerator.driveInfo();
        switch (drive_info.type())
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

        item->set_path(drive_info.path().u8string());
        item->set_name(drive_info.volumeName());
        item->set_total_space(drive_info.totalSpace());
        item->set_free_space(drive_info.freeSpace());
    }

    std::filesystem::path desktop_path;
    if (base::BasePaths::userDesktop(&desktop_path))
    {
        proto::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(desktop_path.u8string());
        item->set_total_space(-1);
        item->set_free_space(-1);
    }

    std::filesystem::path home_path;
    if (base::BasePaths::userHome(&home_path))
    {
        proto::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(home_path.u8string());
        item->set_total_space(-1);
        item->set_free_space(-1);
    }

    if (reply.drive_list().item_size() == 0)
        reply.set_status(proto::FileReply::STATUS_NO_DRIVES_FOUND);
    else
        reply.set_status(proto::FileReply::STATUS_SUCCESS);

    return reply;
}

proto::FileReply FileWorker::doFileListRequest(const proto::FileListRequest& request)
{
    proto::FileReply reply;

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    std::filesystem::file_status status = std::filesystem::status(path, ignored_code);

    if (!std::filesystem::exists(status))
    {
        reply.set_status(proto::FileReply::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    if (!std::filesystem::is_directory(status))
    {
        reply.set_status(proto::FileReply::STATUS_INVALID_PATH_NAME);
        return reply;
    }

    FileEnumerator enumerator(path);

    while (!enumerator.isAtEnd())
    {
        const FileEnumerator::FileInfo& file_info = enumerator.fileInfo();

        proto::FileList::Item* item = reply.mutable_file_list()->add_item();
        item->set_name(file_info.name().u8string());
        item->set_size(file_info.size());
        item->set_modification_time(file_info.lastWriteTime());
        item->set_is_directory(file_info.isDirectory());

        enumerator.advance();
    }

    reply.set_status(enumerator.status());
    return reply;
}

proto::FileReply FileWorker::doCreateDirectoryRequest(const proto::CreateDirectoryRequest& request)
{
    proto::FileReply reply;

    std::filesystem::path directory_path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (std::filesystem::exists(directory_path, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    if (!std::filesystem::create_directory(directory_path, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::FileReply::STATUS_SUCCESS);
    return reply;
}

proto::FileReply FileWorker::doRenameRequest(const proto::RenameRequest& request)
{
    proto::FileReply reply;

    std::filesystem::path old_name = std::filesystem::u8path(request.old_name());
    std::filesystem::path new_name = std::filesystem::u8path(request.new_name());

    if (old_name == new_name)
    {
        reply.set_status(proto::FileReply::STATUS_SUCCESS);
        return reply;
    }

    std::error_code ignored_code;
    if (!std::filesystem::exists(old_name, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    if (std::filesystem::exists(new_name, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    std::error_code error_code;
    std::filesystem::rename(old_name, new_name, error_code);

    if (error_code)
    {
        reply.set_status(proto::FileReply::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::FileReply::STATUS_SUCCESS);
    return reply;
}

proto::FileReply FileWorker::doRemoveRequest(const proto::RemoveRequest& request)
{
    proto::FileReply reply;

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (!std::filesystem::exists(path, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
        std::filesystem::perm_options::add,
        ignored_code);

    if (!std::filesystem::remove(path, ignored_code))
    {
        reply.set_status(proto::FileReply::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::FileReply::STATUS_SUCCESS);
    return reply;
}

proto::FileReply FileWorker::doDownloadRequest(const proto::DownloadRequest& request)
{
    proto::FileReply reply;

    packetizer_ = FilePacketizer::create(std::filesystem::u8path(request.path()));
    if (!packetizer_)
        reply.set_status(proto::FileReply::STATUS_FILE_OPEN_ERROR);
    else
        reply.set_status(proto::FileReply::STATUS_SUCCESS);

    return reply;
}

proto::FileReply FileWorker::doUploadRequest(const proto::UploadRequest& request)
{
    proto::FileReply reply;

    std::filesystem::path file_path = std::filesystem::u8path(request.path());

    do
    {
        if (!request.overwrite())
        {
            std::error_code ignored_code;
            if (std::filesystem::exists(file_path, ignored_code))
            {
                reply.set_status(proto::FileReply::STATUS_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(file_path, request.overwrite());
        if (!depacketizer_)
        {
            reply.set_status(proto::FileReply::STATUS_FILE_CREATE_ERROR);
            break;
        }

        reply.set_status(proto::FileReply::STATUS_SUCCESS);
    }
    while (false);

    return reply;
}

proto::FileReply FileWorker::doPacketRequest(const proto::FilePacketRequest& request)
{
    proto::FileReply reply;

    if (!packetizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply.set_status(proto::FileReply::STATUS_UNKNOWN);
        LOG(LS_WARNING) << "Unexpected file packet request";
    }
    else
    {
        std::unique_ptr<proto::FilePacket> packet = packetizer_->readNextPacket(request);
        if (!packet)
        {
            reply.set_status(proto::FileReply::STATUS_FILE_READ_ERROR);
            packetizer_.reset();
        }
        else
        {
            if (packet->flags() & proto::FilePacket::LAST_PACKET)
                packetizer_.reset();

            reply.set_status(proto::FileReply::STATUS_SUCCESS);
            reply.set_allocated_packet(packet.release());
        }
    }

    return reply;
}

proto::FileReply FileWorker::doPacket(const proto::FilePacket& packet)
{
    proto::FileReply reply;

    if (!depacketizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply.set_status(proto::FileReply::STATUS_UNKNOWN);
        LOG(LS_WARNING) << "Unexpected file packet";
    }
    else
    {
        if (!depacketizer_->writeNextPacket(packet))
        {
            reply.set_status(proto::FileReply::STATUS_FILE_WRITE_ERROR);
            depacketizer_.reset();
        }
        else
        {
            reply.set_status(proto::FileReply::STATUS_SUCCESS);
        }

        if (packet.flags() & proto::FilePacket::LAST_PACKET)
            depacketizer_.reset();
    }

    return reply;
}

} // namespace common
