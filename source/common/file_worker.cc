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

#include "common/file_worker.h"

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
#include <QJniEnvironment>
#include <QJniObject>
#endif // defined(Q_OS_ANDROID)

namespace {

#if defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
// Builds the drive list shown for the local Android device: the user storages and a few quick
// folders, instead of the raw POSIX mount points reported by QStorageInfo.
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

    if (!internal_path.isEmpty())
    {
        proto::file_transfer::DriveList::Item* item = drive_list->add_item();
        item->set_type(proto::file_transfer::DriveList::Item::TYPE_INTERNAL_STORAGE);
        item->set_path(internal_path.toStdString());
    }

    // Removable storage: the second and later entries of getExternalFilesDirs point at SD cards. The
    // returned path is the app sandbox on that volume, so trim it back to the volume root.
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
    {
        QJniObject dirs = context.callObjectMethod(
            "getExternalFilesDirs", "(Ljava/lang/String;)[Ljava/io/File;", nullptr);
        if (dirs.isValid())
        {
            QJniEnvironment env;
            jobjectArray array = dirs.object<jobjectArray>();
            const jsize count = env->GetArrayLength(array);

            for (jsize i = 1; i < count; ++i)
            {
                QJniObject dir = QJniObject::fromLocalRef(env->GetObjectArrayElement(array, i));
                if (!dir.isValid())
                    continue;

                const QString path = dir.callObjectMethod(
                    "getAbsolutePath", "()Ljava/lang/String;").toString();
                const int android_index = path.indexOf("/Android/");
                if (android_index <= 0)
                    continue;

                proto::file_transfer::DriveList::Item* item = drive_list->add_item();
                item->set_type(proto::file_transfer::DriveList::Item::TYPE_SD_CARD);
                item->set_path(path.left(android_index).toStdString());
            }
        }
    }

    if (internal_path.isEmpty())
        return;

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

    for (const auto& folder : kQuickFolders)
    {
        const QString path = internal_path + folder.sub_path;
        if (!QDir(path).exists())
            continue;

        proto::file_transfer::DriveList::Item* item = drive_list->add_item();
        item->set_type(folder.type);
        item->set_path(path.toStdString());
    }
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

} // namespace

//--------------------------------------------------------------------------------------------------
FileWorker::FileWorker(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileWorker::~FileWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRequest(const FileTask& task)
{
    FileTask localTask(task);

    proto::file_transfer::Reply reply;
    doRequest(localTask.request(), &reply);

    localTask.onReply(std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRequest(
    const proto::file_transfer::Request& request, proto::file_transfer::Reply* reply)
{
    // The reply always mirrors the request id back to the sender.
    reply->set_request_id(request.request_id());

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
void FileWorker::doDriveListRequest(proto::file_transfer::Reply* reply)
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
void FileWorker::doFileListRequest(
    const proto::file_transfer::ListRequest& request, proto::file_transfer::Reply* reply)
{
    QString path = QString::fromStdString(request.path());
    if (!QFileInfo::exists(path))
    {
        LOG(WARNING) << "Requested directory not exists:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    QFileInfo dir_info(path);
    if (!dir_info.isDir())
    {
        LOG(WARNING) << "Requested directory is not directory:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_INVALID_PATH_NAME);
        return;
    }

    proto::file_transfer::List* file_list = reply->mutable_file_list();

    for (FileEnumerator enumerator(path); !enumerator.isAtEnd(); enumerator.advance())
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
void FileWorker::doCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request, proto::file_transfer::Reply* reply)
{
    QString directory_path = QString::fromStdString(request.path());

    if (QFileInfo::exists(directory_path))
    {
        LOG(WARNING) << "Directory already exists:" << directory_path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
        return;
    }

    if (!QDir().mkdir(directory_path))
    {
        LOG(WARNING) << "Unable to create directory:" << directory_path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRenameRequest(
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

    if (!QFileInfo::exists(old_name))
    {
        LOG(WARNING) << "File being renamed does not exist or cannot be accessed:" << old_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    if (QFileInfo::exists(new_name))
    {
        LOG(WARNING) << "File with the new name already exists:" << new_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
        return;
    }

    if (!QFile::rename(old_name, new_name))
    {
        LOG(WARNING) << "Failed to rename file from" << old_name << "to" << new_name;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
        return;
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    return;
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doRemoveRequest(
    const proto::file_transfer::RemoveRequest& request, proto::file_transfer::Reply* reply)
{
    QString path = QString::fromStdString(request.path());

    QFileInfo path_info(path);
    if (!path_info.exists(path))
    {
        LOG(WARNING) << "Path to be deleted was not found or could not be accessed:" << path;
        reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND);
        return;
    }

    if (path_info.isDir())
    {
        if (!QDir().rmdir(path))
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

        QFile::setPermissions(path, permissions);

        if (!QFile::remove(path))
        {
            LOG(WARNING) << "Unable to remove file:" << path;
            reply->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
            return;
        }
    }

    reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doDownloadRequest(
    const proto::file_transfer::DownloadRequest& request, proto::file_transfer::Reply* reply)
{
    packetizer_ = FilePacketizer::create(QString::fromStdString(request.path()));
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
void FileWorker::doUploadRequest(
    const proto::file_transfer::UploadRequest& request, proto::file_transfer::Reply* reply)
{
    QString file_path = QString::fromStdString(request.path());

    do
    {
        if (!request.overwrite())
        {
            if (QFileInfo::exists(file_path))
            {
                LOG(WARNING) << "File already exists:" << file_path;
                reply->set_error_code(proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS);
                break;
            }
        }

        depacketizer_ = FileDepacketizer::create(file_path, request.overwrite());
        if (!depacketizer_)
        {
            LOG(WARNING) << "Unable to create file:" << file_path;
            reply->set_error_code(proto::file_transfer::ERROR_CODE_FILE_CREATE_ERROR);
            break;
        }

        reply->set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    }
    while (false);
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doPacketRequest(
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
void FileWorker::doPacket(
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
