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

#include "host/file_worker.h"

#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include <QStorageInfo>

#if defined(Q_OS_WIN)
#include "base/win/file_enumerator.h"
#endif // defined(Q_OS_WIN)

#include "host/file_platform_util.h"

namespace aspia {

FileWorker::FileWorker(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

proto::file_transfer::Reply FileWorker::doRequest(const proto::file_transfer::Request& request)
{
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
        return doPacketRequest();
    }
    else if (request.has_packet())
    {
        return doPacket(request.packet());
    }
    else
    {
        proto::file_transfer::Reply reply;
        reply.set_status(proto::file_transfer::STATUS_INVALID_REQUEST);
        return reply;
    }
}

void FileWorker::executeRequest(FileRequest* request)
{
    request->sendReply(doRequest(request->request()));
    delete request;
}

proto::file_transfer::Reply FileWorker::doDriveListRequest()
{
    proto::file_transfer::Reply reply;

    for (const auto& volume : QStorageInfo::mountedVolumes())
    {
        QString root_path = volume.rootPath();

        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(FilePlatformUtil::driveType(root_path));
        item->set_path(root_path.toStdString());
    }

    QString desktop_path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktop_path.isEmpty())
    {
        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(desktop_path.toStdString());
    }

    QString home_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home_path.isEmpty())
    {
        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(home_path.toStdString());
    }

    if (reply.drive_list().item_size() == 0)
        reply.set_status(proto::file_transfer::STATUS_NO_DRIVES_FOUND);
    else
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);

    return reply;
}

proto::file_transfer::Reply FileWorker::doFileListRequest(
    const proto::file_transfer::FileListRequest& request)
{
    proto::file_transfer::Reply reply;

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    std::filesystem::file_status status = std::filesystem::status(path, ignored_code);

    if (!std::filesystem::exists(status))
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    if (!std::filesystem::is_directory(status))
    {
        reply.set_status(proto::file_transfer::STATUS_INVALID_PATH_NAME);
        return reply;
    }

#if defined(Q_OS_WIN)
    for (FileEnumerator enumerator(path); !enumerator.isAtEnd(); enumerator.advance())
    {
        const FileEnumerator::FileInfo& file_info = enumerator.fileInfo();

        proto::file_transfer::FileList::Item* item = reply.mutable_file_list()->add_item();
        item->set_name(file_info.name().u8string());
        item->set_size(file_info.size());
        item->set_modification_time(file_info.lastWriteTime());
        item->set_is_directory(file_info.isDirectory());
    }
#else
    for (const auto& directory_entry : std::filesystem::directory_iterator(path))
    {
        proto::file_transfer::FileList::Item* item = reply.mutable_file_list()->add_item();

        item->set_name(directory_entry.path().filename().u8string());
        item->set_size(directory_entry.file_size());
        item->set_is_directory(directory_entry.is_directory());

        std::filesystem::file_time_type file_time = directory_entry.last_write_time();
        time_t time = decltype(file_time)::clock::to_time_t(file_time);
        item->set_modification_time(time);
    }
#endif

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request)
{
    proto::file_transfer::Reply reply;

    std::filesystem::path directory_path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (std::filesystem::exists(directory_path, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    if (!std::filesystem::create_directory(directory_path, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doRenameRequest(
    const proto::file_transfer::RenameRequest& request)
{
    proto::file_transfer::Reply reply;

    std::filesystem::path old_name = std::filesystem::u8path(request.old_name());
    std::filesystem::path new_name = std::filesystem::u8path(request.new_name());

    if (old_name == new_name)
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        return reply;
    }

    std::error_code ignored_code;
    if (!std::filesystem::exists(old_name, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    if (std::filesystem::exists(new_name, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    std::error_code error_code;
    std::filesystem::rename(old_name, new_name, error_code);

    if (error_code)
    {
        reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doRemoveRequest(
    const proto::file_transfer::RemoveRequest& request)
{
    proto::file_transfer::Reply reply;

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (!std::filesystem::exists(path, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
        std::filesystem::perm_options::add,
        ignored_code);

    if (!std::filesystem::remove(path, ignored_code))
    {
        reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
        return reply;
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doDownloadRequest(
    const proto::file_transfer::DownloadRequest& request)
{
    proto::file_transfer::Reply reply;

    packetizer_ = FilePacketizer::create(std::filesystem::u8path(request.path()));
    if (!packetizer_)
        reply.set_status(proto::file_transfer::STATUS_FILE_OPEN_ERROR);
    else
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);

    return reply;
}

proto::file_transfer::Reply FileWorker::doUploadRequest(
    const proto::file_transfer::UploadRequest& request)
{
    proto::file_transfer::Reply reply;

    std::filesystem::path file_path = std::filesystem::u8path(request.path());

    do
    {
        if (!request.overwrite())
        {
            std::error_code ignored_code;
            if (std::filesystem::exists(file_path, ignored_code))
            {
                reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(file_path, request.overwrite());
        if (!depacketizer_)
        {
            reply.set_status(proto::file_transfer::STATUS_FILE_CREATE_ERROR);
            break;
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }
    while (false);

    return reply;
}

proto::file_transfer::Reply FileWorker::doPacketRequest()
{
    proto::file_transfer::Reply reply;

    if (!packetizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply.set_status(proto::file_transfer::STATUS_UNKNOWN);
        qWarning("Unexpected file packet request");
    }
    else
    {
        std::unique_ptr<proto::file_transfer::Packet> packet =
            packetizer_->readNextPacket();
        if (!packet)
        {
            reply.set_status(proto::file_transfer::STATUS_FILE_READ_ERROR);
        }
        else
        {
            if (packet->flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
                packetizer_.reset();

            reply.set_status(proto::file_transfer::STATUS_SUCCESS);
            reply.set_allocated_packet(packet.release());
        }
    }

    return reply;
}

proto::file_transfer::Reply FileWorker::doPacket(const proto::file_transfer::Packet& packet)
{
    proto::file_transfer::Reply reply;

    if (!depacketizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply.set_status(proto::file_transfer::STATUS_UNKNOWN);
        qWarning("Unexpected file packet");
    }
    else
    {
        if (!depacketizer_->writeNextPacket(packet))
            reply.set_status(proto::file_transfer::STATUS_FILE_WRITE_ERROR);
        else
            reply.set_status(proto::file_transfer::STATUS_SUCCESS);

        if (packet.flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
            depacketizer_.reset();
    }

    return reply;
}

} // namespace aspia
