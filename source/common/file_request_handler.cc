//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/file_request_handler.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#include "base/logging.h"
#include "base/files/file_enumerator.h"
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_task.h"
#include "proto/file_transfer.h"

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>

#include "common/android/granted_folders.h"
#endif // defined(Q_OS_ANDROID)

namespace {

#if defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
// The drive list of the local Android device is the folders the user granted. The usual folders of
// the internal storage are marked by their type.
void addAndroidDrives(proto::file_transfer::DriveList* drive_list)
{
    QJniObject ext_dir = QJniObject::callStaticObjectMethod(
        "android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
    QString internal_path;
    if (ext_dir.isValid())
    {
        internal_path = ext_dir.callObjectMethod(
            "getAbsolutePath", "()Ljava/lang/String;").toString();
    }

    const struct
    {
        const char* sub_path;
        proto::file_transfer::DriveList::Item::Type type;
    } kQuickFolders[] =
    {
        { "/Download",  proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER },
        { "/DCIM",      proto::file_transfer::DriveList::Item::TYPE_CAMERA_FOLDER },
        { "/Pictures",  proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER },
        { "/Documents", proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER },
    };

    const QList<GrantedFolders::Folder> folders = GrantedFolders::list();
    for (const GrantedFolders::Folder& folder : folders)
    {
        proto::file_transfer::DriveList::Item* item = drive_list->add_item();
        item->set_type(proto::file_transfer::DriveList::Item::TYPE_FOLDER);
        item->set_path(folder.path.toStdString());

        if (internal_path.isEmpty())
            continue;

        for (const auto& quick_folder : kQuickFolders)
        {
            if (folder.path == internal_path + quick_folder.sub_path)
                item->set_type(quick_folder.type);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Returns true when every path of |request| lies inside a granted folder.
bool isGrantedRequest(const proto::file_transfer::Request& request)
{
    QStringList paths;

    if (request.has_file_list_request())
    {
        paths.append(QString::fromStdString(request.file_list_request().path()));
    }
    else if (request.has_create_directory_request())
    {
        paths.append(QString::fromStdString(request.create_directory_request().path()));
    }
    else if (request.has_rename_request())
    {
        paths.append(QString::fromStdString(request.rename_request().old_name()));
        paths.append(QString::fromStdString(request.rename_request().new_name()));
    }
    else if (request.has_remove_request())
    {
        paths.append(QString::fromStdString(request.remove_request().path()));
    }
    else if (request.has_download_request())
    {
        paths.append(QString::fromStdString(request.download_request().path()));
    }
    else if (request.has_upload_request())
    {
        paths.append(QString::fromStdString(request.upload_request().path()));
    }

    for (const QString& path : std::as_const(paths))
    {
        if (!GrantedFolders::contains(path))
            return false;
    }

    return true;
}

#else // defined(Q_OS_ANDROID)

#if defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// QStorageInfo lists every POSIX mount on Linux - snap squashfs images, tmpfs, the boot partition,
// pseudo filesystems and so on. Keep only the volumes a user would actually browse.
bool isUserVisibleVolume(const QStorageInfo& volume)
{
    if (!volume.isValid() || !volume.isReady())
        return false;

    static const char* const kPseudoFileSystems[] = {
        "squashfs", "tmpfs", "devtmpfs", "ramfs", "overlay", "proc", "sysfs", "cgroup", "cgroup2",
        "devpts", "mqueue", "hugetlbfs", "debugfs", "tracefs", "securityfs", "pstore", "bpf",
        "configfs", "fusectl", "nsfs", "autofs", "efivarfs", "binfmt_misc"
    };

    const QByteArray fs_type = volume.fileSystemType();
    for (const char* pseudo : kPseudoFileSystems)
    {
        if (fs_type == pseudo)
            return false;
    }

    static const char* const kSystemPaths[] = { "/snap", "/var/snap", "/boot", "/sys", "/proc", "/dev" };

    const QByteArray root = volume.rootPath().toUtf8();
    for (const char* prefix : kSystemPaths)
    {
        QByteArray path_prefix(prefix);
        if (root == path_prefix)
            return false;

        path_prefix += '/';
        if (root.startsWith(path_prefix))
            return false;
    }

    return true;
}
#endif // defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
// Builds the drive list for the desktop: the mounted volumes plus the Desktop and Home folders.
void addDesktopDrives(proto::file_transfer::DriveList* drive_list)
{
    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const auto& volume : std::as_const(volumes))
    {
#if defined(Q_OS_LINUX)
        if (!isUserVisibleVolume(volume))
            continue;
#endif // defined(Q_OS_LINUX)

        proto::file_transfer::DriveList::Item* item = drive_list->add_item();

#if defined(Q_OS_WINDOWS)
        switch (GetDriveTypeW(qUtf16Printable(volume.rootPath())))
        {
            case DRIVE_REMOVABLE:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_REMOVABLE);
                break;

            case DRIVE_FIXED:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_FIXED);
                break;

            case DRIVE_REMOTE:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_REMOTE);
                break;

            case DRIVE_CDROM:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_CDROM);
                break;

            case DRIVE_RAMDISK:
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_RAM);
                break;

            default:
                break;
        }
#else
        item->set_type(proto::file_transfer::DriveList::Item::TYPE_FIXED);
#endif

        item->set_path(volume.rootPath().toStdString());
    }

    const struct
    {
        QStandardPaths::StandardLocation location;
        proto::file_transfer::DriveList::Item::Type type;
    } kQuickFolders[] =
    {
        { QStandardPaths::DesktopLocation,   proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER },
        { QStandardPaths::HomeLocation,      proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER },
        { QStandardPaths::DownloadLocation,  proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER },
        { QStandardPaths::DocumentsLocation, proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER },
        { QStandardPaths::PicturesLocation,  proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER },
    };

    for (const auto& folder : kQuickFolders)
    {
        const QString path = QStandardPaths::writableLocation(folder.location);
        if (path.isEmpty() || !QDir(path).exists())
            continue;

        proto::file_transfer::DriveList::Item* item = drive_list->add_item();
        item->set_type(folder.type);
        item->set_path(path.toStdString());
    }
}

#endif // defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
// The name the file engine opens |path| by. A path inside a granted folder is reached through the
// document provider of the folder, other paths are used as they are.
QString accessPath(const QString& path)
{
#if defined(Q_OS_ANDROID)
    return GrantedFolders::accessPath(path);
#else
    return path;
#endif // defined(Q_OS_ANDROID)
}

} // namespace

//--------------------------------------------------------------------------------------------------
FileRequestHandler::FileRequestHandler(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileRequestHandler::~FileRequestHandler()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doRequest(const FileTask& task)
{
    FileTask localTask(task);

    proto::file_transfer::Reply reply;
    doRequest(localTask.request(), &reply);

    localTask.onReply(std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doRequest(
    const proto::file_transfer::Request& request, proto::file_transfer::Reply* reply)
{
    // The reply always mirrors the request id back to the sender.
    reply->set_request_id(request.request_id());

#if defined(Q_OS_ANDROID)
    if (!isGrantedRequest(request))
    {
        LOG(WARNING) << "Request for a path outside the granted folders";
        reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
        return;
    }
#endif // defined(Q_OS_ANDROID)

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
        reply->set_error_code(proto::file_transfer::ERROR_CODE_INVALID_REQUEST);
    }
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doDriveListRequest(proto::file_transfer::Reply* reply)
{
    proto::file_transfer::DriveList* drive_list = reply->mutable_drive_list();

#if defined(Q_OS_ANDROID)
    addAndroidDrives(drive_list);
#else
    addDesktopDrives(drive_list);
#endif

    if (drive_list->item_size() == 0)
        reply->set_error_code(proto::file_transfer::ERROR_CODE_NO_DRIVES_FOUND);
    else
        reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doFileListRequest(
    const proto::file_transfer::ListRequest& request, proto::file_transfer::Reply* reply)
{
    QString path = QString::fromStdString(request.path());
    const QString access_path = accessPath(path);

    if (!QFileInfo::exists(access_path))
    {
        LOG(WARNING) << "Requested directory not exists:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    QFileInfo dir_info(access_path);
    if (!dir_info.isDir())
    {
        LOG(WARNING) << "Requested directory is not directory:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_INVALID_PATH_NAME);
        return;
    }

    proto::file_transfer::List* file_list = reply->mutable_file_list();

    for (FileEnumerator enumerator(access_path); !enumerator.isAtEnd(); enumerator.advance())
    {
        const FileEnumerator::FileInfo& file_info = enumerator.fileInfo();
        proto::file_transfer::List::Item* item = file_list->add_item();

        item->set_name(file_info.name().toStdString());
        item->set_size(file_info.size());
        item->set_modification_time(file_info.lastWriteTime());
        item->set_is_directory(file_info.isDirectory());
    }
    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request, proto::file_transfer::Reply* reply)
{
    QString directory_path = QString::fromStdString(request.path());

    if (QFileInfo::exists(accessPath(directory_path)))
    {
        LOG(WARNING) << "Directory already exists:" << directory_path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
        return;
    }

    // A document provider creates a directory only inside its parent directory.
    QFileInfo directory_info(QDir::cleanPath(directory_path));
    if (!QDir(accessPath(directory_info.path())).mkdir(directory_info.fileName()))
    {
        LOG(WARNING) << "Unable to create directory:" << directory_path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doRenameRequest(
    const proto::file_transfer::RenameRequest& request, proto::file_transfer::Reply* reply)
{
    QString old_name = QString::fromStdString(request.old_name());
    QString new_name = QString::fromStdString(request.new_name());

    if (old_name == new_name)
    {
        LOG(WARNING) << "Name of new and old element is equal:" << old_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
        return;
    }

    const QString old_access_path = accessPath(old_name);
    const QString new_access_path = accessPath(new_name);

    if (!QFileInfo::exists(old_access_path))
    {
        LOG(WARNING) << "File being renamed does not exist or cannot be accessed:" << old_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    if (QFileInfo::exists(new_access_path))
    {
        LOG(WARNING) << "File with the new name already exists:" << new_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
        return;
    }

    if (!QFile::rename(old_access_path, new_access_path))
    {
        LOG(WARNING) << "Failed to rename file from" << old_name << "to" << new_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    return;
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doRemoveRequest(
    const proto::file_transfer::RemoveRequest& request, proto::file_transfer::Reply* reply)
{
    QString path = QString::fromStdString(request.path());
    const QString access_path = accessPath(path);

    QFileInfo path_info(access_path);
    if (!path_info.exists(access_path))
    {
        LOG(WARNING) << "Path to be deleted was not found or could not be accessed:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    if (path_info.isDir())
    {
        // A document provider removes a directory only through its parent directory.
        QFileInfo directory_info(QDir::cleanPath(path));
        if (!QDir(accessPath(directory_info.path())).rmdir(directory_info.fileName()))
        {
            LOG(WARNING) << "Unable to remove direcotry:" << path;
            reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
            return;
        }
    }
    else
    {
        QFile::Permissions permissions = path_info.permissions();
        permissions |= QFile::WriteGroup | QFile::WriteOther | QFile::WriteUser | QFile::WriteOwner;

        QFile::setPermissions(access_path, permissions);

        if (!QFile::remove(access_path))
        {
            LOG(WARNING) << "Unable to remove file:" << path;
            reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
            return;
        }
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doDownloadRequest(
    const proto::file_transfer::DownloadRequest& request, proto::file_transfer::Reply* reply)
{
    packetizer_ = FilePacketizer::create(accessPath(QString::fromStdString(request.path())));
    if (!packetizer_)
    {
        LOG(WARNING) << "Unable to open file:" << request.path();
        reply->set_error_code(proto::file_transfer::ERROR_CODE_FILE_OPEN_ERROR);
    }
    else
    {
        reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    }
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doUploadRequest(
    const proto::file_transfer::UploadRequest& request, proto::file_transfer::Reply* reply)
{
    QString file_path = QString::fromStdString(request.path());
    const QString access_path = accessPath(file_path);

    do
    {
        if (!request.overwrite())
        {
            if (QFileInfo::exists(access_path))
            {
                LOG(WARNING) << "File already exists:" << file_path;
                reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(access_path, request.overwrite());
        if (!depacketizer_)
        {
            if (!request.overwrite() && QFileInfo::exists(access_path))
            {
                LOG(WARNING) << "File already exists:" << file_path;
                reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
                break;
            }

            LOG(WARNING) << "Unable to create file:" << file_path;
            reply->set_error_code(proto::file_transfer::ERROR_CODE_FILE_CREATE_ERROR);
            break;
        }

        reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    }
    while (false);
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doPacketRequest(
    const proto::file_transfer::PacketRequest& request, proto::file_transfer::Reply* reply)
{
    if (!packetizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::file_transfer::ERROR_CODE_UNKNOWN);
        LOG(ERROR) << "Unexpected file packet request";
    }
    else
    {
        std::unique_ptr<proto::file_transfer::Packet> packet = packetizer_->readNextPacket(request);
        if (!packet)
        {
            reply->set_error_code(proto::file_transfer::ERROR_CODE_FILE_READ_ERROR);
            packetizer_.reset();
        }
        else
        {
            if (packet->flags() & proto::file_transfer::Packet::LAST_PACKET)
                packetizer_.reset();

            reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
            reply->set_allocated_packet(packet.release());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void FileRequestHandler::doPacket(
    const proto::file_transfer::Packet& packet, proto::file_transfer::Reply* reply)
{
    if (!depacketizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::file_transfer::ERROR_CODE_UNKNOWN);
        LOG(ERROR) << "Unexpected file packet";
    }
    else
    {
        if (!depacketizer_->writeNextPacket(packet))
        {
            reply->set_error_code(proto::file_transfer::ERROR_CODE_FILE_WRITE_ERROR);
            depacketizer_.reset();
        }
        else
        {
            reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
        }

        if (packet.flags() & proto::file_transfer::Packet::LAST_PACKET)
            depacketizer_.reset();
    }
}
