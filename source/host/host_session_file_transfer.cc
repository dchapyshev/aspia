//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"

#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QStorageInfo>

#include "base/process/process.h"
#include "client/file_platform_util.h"
#include "ipc/pipe_channel_proxy.h"
#include "protocol/message_serialization.h"
#include "proto/auth_session.pb.h"

namespace aspia {

void HostSessionFileTransfer::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (ipc_channel_)
    {
        ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

        uint32_t user_data = Process::Current().Pid();

        if (ipc_channel_->Connect(user_data))
        {
            OnIpcChannelConnect(user_data);
            ipc_channel_proxy_->WaitForDisconnect();
        }

        ipc_channel_.reset();
    }
}

void HostSessionFileTransfer::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(user_data);

    if (session_type != proto::auth::SESSION_TYPE_FILE_TRANSFER)
    {
        qFatal("Invalid session type passed");
        return;
    }

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionFileTransfer::OnIpcChannelMessage(const QByteArray& buffer)
{
    proto::file_transfer::Request message;

    if (!ParseMessage(buffer, message))
    {
        ipc_channel_proxy_->Disconnect();
        return;
    }

    if (message.has_drive_list_request())
    {
        ReadDriveListRequest();
    }
    else if (message.has_file_list_request())
    {
        ReadFileListRequest(message.file_list_request());
    }
    else if (message.has_create_directory_request())
    {
        ReadCreateDirectoryRequest(message.create_directory_request());
    }
    else if (message.has_rename_request())
    {
        ReadRenameRequest(message.rename_request());
    }
    else if (message.has_remove_request())
    {
        ReadRemoveRequest(message.remove_request());
    }
    else if (message.has_upload_request())
    {
        ReadFileUploadRequest(message.upload_request());
    }
    else if (message.has_packet())
    {
        if (!ReadFilePacket(message.packet()))
            ipc_channel_proxy_->Disconnect();
    }
    else if (message.has_download_request())
    {
        ReadFileDownloadRequest(message.download_request());
    }
    else if (message.has_packet_request())
    {
        if (!ReadFilePacketRequest())
            ipc_channel_proxy_->Disconnect();
    }
    else
    {
        qWarning("Unknown message from client");
        ipc_channel_proxy_->Disconnect();
    }
}

void HostSessionFileTransfer::SendReply(const proto::file_transfer::Reply& reply)
{
    ipc_channel_proxy_->Send(
        SerializeMessage(reply),
        std::bind(&HostSessionFileTransfer::OnReplySended, this));
}

void HostSessionFileTransfer::OnReplySended()
{
    // Receive next request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionFileTransfer::ReadDriveListRequest()
{
    proto::file_transfer::Reply reply;

    for (const auto& volume : QStorageInfo::mountedVolumes())
    {
        QString root_path = volume.rootPath();

        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(FilePlatformUtil::driveType(root_path));
        item->set_path(root_path.toUtf8());
    }

    QString desktop_path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktop_path.isEmpty())
    {
        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(desktop_path.toUtf8());
    }

    QString home_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home_path.isEmpty())
    {
        proto::file_transfer::DriveList::Item* item = reply.mutable_drive_list()->add_item();

        item->set_type(proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(home_path.toUtf8());
    }

    if (!reply.drive_list().item_size())
        reply.set_status(proto::file_transfer::STATUS_NO_DRIVES_FOUND);
    else
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileListRequest(
    const proto::file_transfer::FileListRequest& request)
{
    proto::file_transfer::Reply reply;

    QString directory_path = QString::fromUtf8(request.path().c_str(), request.path().size());

    QDir directory(directory_path);
    if (!directory.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
    }
    else
    {
        directory.setFilter(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot |
                            QDir::System | QDir::Hidden);
        directory.setSorting(QDir::Name | QDir::DirsFirst);

        QFileInfoList info_list = directory.entryInfoList();

        for (const auto& info : info_list)
        {
            proto::file_transfer::FileList::Item* item = reply.mutable_file_list()->add_item();

            item->set_name(info.fileName().toUtf8());
            item->set_size(info.size());
            item->set_modification_time(info.lastModified().toTime_t());
            item->set_is_directory(info.isDir());
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request)
{
    proto::file_transfer::Reply reply;

    QString directory_path = QString::fromUtf8(request.path().c_str(), request.path().size());

    QFileInfo file_info(directory_path);
    if (!file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
    }
    else
    {
        QDir directory;
        if (!directory.mkdir(directory_path))
            reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
        else
            reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRenameRequest(const proto::file_transfer::RenameRequest& request)
{
    proto::file_transfer::Reply reply;

    QString old_name = QString::fromUtf8(request.old_name().c_str(), request.old_name().size());
    QString new_name = QString::fromUtf8(request.new_name().c_str(), request.new_name().size());

    if (old_name == new_name)
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }
    else
    {
        QFileInfo old_file_info(old_name);
        if (old_file_info.exists())
        {
            reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
        }
        else
        {
            QFileInfo new_file_info(new_name);
            if (new_file_info.exists())
            {
                reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
            }
            else
            {
                if (old_file_info.isDir())
                {
                    if (!QDir().rename(old_name, new_name))
                        reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
                    else
                        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
                }
                else
                {
                    if (!QFile(old_name).rename(new_name))
                        reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
                    else
                        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
                }
            }
        }
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRemoveRequest(const proto::file_transfer::RemoveRequest& request)
{
    proto::file_transfer::Reply reply;

    QString path = QString::fromUtf8(request.path().c_str(), request.path().size());

    QFileInfo file_info(path);
    if (!file_info.exists())
    {
        reply.set_status(proto::file_transfer::STATUS_PATH_NOT_FOUND);
    }
    else
    {
        if (file_info.isDir())
        {
            if (!QDir().rmdir(path))
                reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            else
                reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        }
        else
        {
            if (!QFile::remove(path))
                reply.set_status(proto::file_transfer::STATUS_ACCESS_DENIED);
            else
                reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        }
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileUploadRequest(
    const proto::file_transfer::UploadRequest& request)
{
    proto::file_transfer::Reply reply;

    QString file_path = QString::fromUtf8(request.path().c_str(), request.path().size());

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

        file_depacketizer_ = FileDepacketizer::Create(file_path, request.overwrite());
        if (!file_depacketizer_)
        {
            reply.set_status(proto::file_transfer::STATUS_FILE_CREATE_ERROR);
            break;
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }
    while (false);

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacket(const proto::file_transfer::Packet& file_packet)
{
    if (!file_depacketizer_)
    {
        qWarning("Unexpected file packet");
        return false;
    }

    proto::file_transfer::Reply reply;

    if (!file_depacketizer_->ReadNextPacket(file_packet))
    {
        reply.set_status(proto::file_transfer::STATUS_FILE_WRITE_ERROR);
    }
    else
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }

    if (file_packet.flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
    {
        file_depacketizer_.reset();
    }

    SendReply(reply);
    return true;
}

void HostSessionFileTransfer::ReadFileDownloadRequest(
    const proto::file_transfer::DownloadRequest& request)
{
    proto::file_transfer::Reply reply;

    QString file_path = QString::fromUtf8(request.path().c_str(), request.path().size());

    file_packetizer_ = FilePacketizer::Create(file_path);
    if (!file_packetizer_)
    {
        reply.set_status(proto::file_transfer::STATUS_FILE_OPEN_ERROR);
    }
    else
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacketRequest()
{
    if (!file_packetizer_)
    {
        qWarning("Unexpected download data request");
        return false;
    }

    proto::file_transfer::Reply reply;

    std::unique_ptr<proto::file_transfer::Packet> packet =
        file_packetizer_->CreateNextPacket();
    if (!packet)
    {
        reply.set_status(proto::file_transfer::STATUS_FILE_READ_ERROR);
    }
    else
    {
        if (packet->flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
        {
            file_packetizer_.reset();
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        reply.set_allocated_packet(packet.release());
    }

    SendReply(reply);
    return true;
}

} // namespace aspia
