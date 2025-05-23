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
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

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
void FileWorker::doRequest(const FileTask& task)
{
    FileTask localTask(task);

    proto::FileReply reply;
    doRequest(localTask.request(), &reply);

    localTask.onReply(std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRequest(const proto::FileRequest& request, proto::FileReply* reply)
{
#if defined(Q_OS_WINDOWS)
    // We send a notification to the system that it is used to prevent the screen saver, going into
    // hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
#endif // defined(Q_OS_WINDOWS)

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

    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const auto& volume : std::as_const(volumes))
    {
        proto::DriveList::Item* item = drive_list->add_item();

#if defined(Q_OS_WINDOWS)
        switch (GetDriveTypeW(reinterpret_cast<const wchar_t*>(volume.rootPath().utf16())))
        {
            case DRIVE_REMOVABLE:
                item->set_type(proto::DriveList::Item::TYPE_REMOVABLE);
                break;

            case DRIVE_FIXED:
                item->set_type(proto::DriveList::Item::TYPE_FIXED);
                break;

            case DRIVE_REMOTE:
                item->set_type(proto::DriveList::Item::TYPE_REMOTE);
                break;

            case DRIVE_CDROM:
                item->set_type(proto::DriveList::Item::TYPE_CDROM);
                break;

            case DRIVE_RAMDISK:
                item->set_type(proto::DriveList::Item::TYPE_RAM);
                break;

            default:
                break;
        }
#else
        item->set_type(proto::DriveList::Item::TYPE_FIXED);
#endif

        item->set_path(volume.rootPath().toStdString());
    }

    QString desktop_path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktop_path.isEmpty())
    {
        proto::DriveList::Item* item = drive_list->add_item();
        item->set_type(proto::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(desktop_path.toStdString());
    }

    QString home_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home_path.isEmpty())
    {
        proto::DriveList::Item* item = drive_list->add_item();
        item->set_type(proto::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(home_path.toStdString());
    }

    if (drive_list->item_size() == 0)
        reply->set_error_code(proto::FILE_ERROR_NO_DRIVES_FOUND);
    else
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doFileListRequest(const proto::FileListRequest& request, proto::FileReply* reply)
{
    QString path = QString::fromStdString(request.path());
    if (!QFileInfo::exists(path))
    {
        LOG(LS_WARNING) << "Requested directory not exists: " << path;
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return;
    }

    QFileInfo dir_info(path);
    if (!dir_info.isDir())
    {
        LOG(LS_WARNING) << "Requested directory is not directory: " << path;
        reply->set_error_code(proto::FILE_ERROR_INVALID_PATH_NAME);
        return;
    }

    proto::FileList* file_list = reply->mutable_file_list();

    QDir dir(path);
    QFileInfoList dir_items = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    for (const auto& dir_item : std::as_const(dir_items))
    {
        proto::FileList::Item* item = file_list->add_item();

        LOG(LS_INFO) << "ITEM: " << dir_item.fileName();

        item->set_name(dir_item.fileName().toStdString());
        item->set_size(dir_item.size());
        item->set_modification_time(dir_item.lastModified().toTime_t());
        item->set_is_directory(dir_item.isDir());
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request, proto::FileReply* reply)
{
    QString directory_path = QString::fromStdString(request.path());

    if (QFileInfo::exists(directory_path))
    {
        LOG(LS_WARNING) << "Directory already exists: " << directory_path;
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return;
    }

    if (!QDir().mkdir(directory_path))
    {
        LOG(LS_WARNING) << "Unable to create directory: " << directory_path;
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRenameRequest(const proto::RenameRequest& request, proto::FileReply* reply)
{
    QString old_name = QString::fromStdString(request.old_name());
    QString new_name = QString::fromStdString(request.new_name());

    if (old_name == new_name)
    {
        LOG(LS_WARNING) << "Name of new and old element is equal: " << old_name;
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
        return;
    }

    if (!QFileInfo::exists(old_name))
    {
        LOG(LS_WARNING) << "File being renamed does not exist or cannot be accessed: " << old_name;
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return;
    }

    if (QFileInfo::exists(new_name))
    {
        LOG(LS_WARNING) << "File with the new name already exists: " << new_name;
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return;
    }

    if (!QFile::rename(old_name, new_name))
    {
        LOG(LS_WARNING) << "Failed to rename file from " << old_name << " to " << new_name;
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    return;
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRemoveRequest(const proto::RemoveRequest& request, proto::FileReply* reply)
{
    QString path = QString::fromStdString(request.path());

    QFileInfo path_info(path);
    if (!path_info.exists(path))
    {
        LOG(LS_WARNING) << "Path to be deleted was not found or could not be accessed: " << path;
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return;
    }

    if (path_info.isDir())
    {
        if (!QDir().rmdir(path))
        {
            LOG(LS_WARNING) << "Unable to remove direcotry: " << path;
            reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
            return;
        }
    }
    else
    {
        QFile::Permissions permissions = path_info.permissions();
        permissions |= QFile::WriteGroup | QFile::WriteOther | QFile::WriteUser | QFile::WriteOwner;

        QFile::setPermissions(path, permissions);

        if (!QFile::remove(path))
        {
            LOG(LS_WARNING) << "Unable to remove file: " << path;
            reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
            return;
        }
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doDownloadRequest(const proto::DownloadRequest& request, proto::FileReply* reply)
{
    packetizer_ = FilePacketizer::create(QString::fromStdString(request.path()));
    if (!packetizer_)
    {
        LOG(LS_WARNING) << "Unable to open file: " << request.path();
        reply->set_error_code(proto::FILE_ERROR_FILE_OPEN_ERROR);
    }
    else
    {
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    }
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doUploadRequest(const proto::UploadRequest& request, proto::FileReply* reply)
{
    QString file_path = QString::fromStdString(request.path());

    do
    {
        if (!request.overwrite())
        {
            if (QFileInfo::exists(file_path))
            {
                LOG(LS_WARNING) << "File already exists: " << file_path;
                reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(file_path, request.overwrite());
        if (!depacketizer_)
        {
            LOG(LS_WARNING) << "Unable to create file: " << file_path;
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
