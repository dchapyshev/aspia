//
// PROJECT:         Aspia
// FILE:            host/file_worker.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_worker.h"

#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include <QStorageInfo>

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

    QDir directory(QString::fromStdString(request.path()));
    if (!directory.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    directory.setFilter(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot |
                        QDir::System | QDir::Hidden);
    directory.setSorting(QDir::Name | QDir::DirsFirst);

    QFileInfoList info_list = directory.entryInfoList();

    for (const auto& info : info_list)
    {
        proto::file_transfer::FileList::Item* item = reply.mutable_file_list()->add_item();

        item->set_name(info.fileName().toStdString());
        item->set_size(info.size());
        item->set_modification_time(info.lastModified().toTime_t());
        item->set_is_directory(info.isDir());
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request)
{
    proto::file_transfer::Reply reply;

    QString directory_path = QString::fromStdString(request.path());

    QFileInfo file_info(directory_path);
    if (file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    QDir directory;
    if (!directory.mkdir(directory_path))
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

    QString old_name = QString::fromStdString(request.old_name());
    QString new_name = QString::fromStdString(request.new_name());

    if (old_name == new_name)
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        return reply;
    }

    QFileInfo old_file_info(old_name);
    if (!old_file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    QFileInfo new_file_info(new_name);
    if (new_file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
        return reply;
    }

    if (old_file_info.isDir())
    {
        if (!QDir().rename(old_name, new_name))
        {
            reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            return reply;
        }
    }
    else
    {
        if (!QFile(old_name).rename(new_name))
        {
            reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            return reply;
        }
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doRemoveRequest(
    const proto::file_transfer::RemoveRequest& request)
{
    proto::file_transfer::Reply reply;

    QString path = QString::fromStdString(request.path());

    QFileInfo file_info(path);
    if (!file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        return reply;
    }

    if (file_info.isDir())
    {
        if (!QDir().rmdir(path))
        {
            reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            return reply;
        }
    }
    else
    {
        QFile file(path);
        file.setPermissions(QFile::ReadOther | QFile::WriteOther);
        if (!file.remove(path))
        {
            reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            return reply;
        }
    }

    reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    return reply;
}

proto::file_transfer::Reply FileWorker::doDownloadRequest(
    const proto::file_transfer::DownloadRequest& request)
{
    proto::file_transfer::Reply reply;

    packetizer_ = FilePacketizer::create(QString::fromStdString(request.path()));
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

    QString file_path = QString::fromStdString(request.path());

    do
    {
        if (!request.overwrite())
        {
            if (QFile(file_path).exists())
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
