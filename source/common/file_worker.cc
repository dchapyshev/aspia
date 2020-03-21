//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/task_runner.h"
#include "base/files/base_paths.h"
#include "build/build_config.h"
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_platform_util.h"
#include "common/file_task.h"

#if defined(OS_WIN)
#include "base/win/drive_enumerator.h"
#include "common/win/file_enumerator.h"
#endif // defined(OS_WIN)

namespace common {

class FileWorker::Impl : public std::enable_shared_from_this<Impl>
{
public:
    explicit Impl(std::shared_ptr<base::TaskRunner> task_runner);
    ~Impl();

    void doTask(std::shared_ptr<FileTask> task);

private:
    std::unique_ptr<proto::FileReply> doRequest(const proto::FileRequest& request);
    std::unique_ptr<proto::FileReply> doDriveListRequest();
    std::unique_ptr<proto::FileReply> doFileListRequest(const proto::FileListRequest& request);
    std::unique_ptr<proto::FileReply> doCreateDirectoryRequest(const proto::CreateDirectoryRequest& request);
    std::unique_ptr<proto::FileReply> doRenameRequest(const proto::RenameRequest& request);
    std::unique_ptr<proto::FileReply> doRemoveRequest(const proto::RemoveRequest& request);
    std::unique_ptr<proto::FileReply> doDownloadRequest(const proto::DownloadRequest& request);
    std::unique_ptr<proto::FileReply> doUploadRequest(const proto::UploadRequest& request);
    std::unique_ptr<proto::FileReply> doPacketRequest(const proto::FilePacketRequest& request);
    std::unique_ptr<proto::FileReply> doPacket(const proto::FilePacket& packet);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<FileDepacketizer> depacketizer_;
    std::unique_ptr<FilePacketizer> packetizer_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

FileWorker::Impl::Impl(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

FileWorker::Impl::~Impl() = default;

void FileWorker::Impl::doTask(std::shared_ptr<FileTask> task)
{
    auto self = shared_from_this();
    task_runner_->postTask([self, task]()
    {
        task->setReply(self->doRequest(task->request()));
    });
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doRequest(const proto::FileRequest& request)
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
        std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();
        reply->set_error_code(proto::FILE_ERROR_INVALID_REQUEST);
        return reply;
    }
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doDriveListRequest()
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    proto::DriveList* drive_list = reply->mutable_drive_list();

    for (base::win::DriveEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::DriveList::Item* item = drive_list->add_item();

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
        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::TYPE_DESKTOP_FOLDER);
        item->set_path(desktop_path.u8string());
        item->set_total_space(-1);
        item->set_free_space(-1);
    }

    std::filesystem::path home_path;
    if (base::BasePaths::userHome(&home_path))
    {
        proto::DriveList::Item* item = drive_list->add_item();

        item->set_type(proto::DriveList::Item::TYPE_HOME_FOLDER);
        item->set_path(home_path.u8string());
        item->set_total_space(-1);
        item->set_free_space(-1);
    }

    if (drive_list->item_size() == 0)
        reply->set_error_code(proto::FILE_ERROR_NO_DRIVES_FOUND);
    else
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);

    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doFileListRequest(
    const proto::FileListRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    std::filesystem::file_status status = std::filesystem::status(path, ignored_code);

    if (!std::filesystem::exists(status))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return reply;
    }

    if (!std::filesystem::is_directory(status))
    {
        reply->set_error_code(proto::FILE_ERROR_INVALID_PATH_NAME);
        return reply;
    }

    proto::FileList* file_list = reply->mutable_file_list();
    FileEnumerator enumerator(path);

    while (!enumerator.isAtEnd())
    {
        const FileEnumerator::FileInfo& file_info = enumerator.fileInfo();

        proto::FileList::Item* item = file_list->add_item();
        item->set_name(file_info.name().u8string());
        item->set_size(file_info.size());
        item->set_modification_time(file_info.lastWriteTime());
        item->set_is_directory(file_info.isDirectory());

        enumerator.advance();
    }

    reply->set_error_code(enumerator.errorCode());
    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    std::filesystem::path directory_path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (std::filesystem::exists(directory_path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return reply;
    }

    if (!std::filesystem::create_directory(directory_path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return reply;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doRenameRequest(
    const proto::RenameRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    std::filesystem::path old_name = std::filesystem::u8path(request.old_name());
    std::filesystem::path new_name = std::filesystem::u8path(request.new_name());

    if (old_name == new_name)
    {
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);
        return reply;
    }

    std::error_code ignored_code;
    if (!std::filesystem::exists(old_name, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return reply;
    }

    if (std::filesystem::exists(new_name, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_ALREADY_EXISTS);
        return reply;
    }

    std::error_code error_code;
    std::filesystem::rename(old_name, new_name, error_code);

    if (error_code)
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return reply;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doRemoveRequest(
    const proto::RemoveRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    std::filesystem::path path = std::filesystem::u8path(request.path());

    std::error_code ignored_code;
    if (!std::filesystem::exists(path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_PATH_NOT_FOUND);
        return reply;
    }

    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
        std::filesystem::perm_options::add,
        ignored_code);

    if (!std::filesystem::remove(path, ignored_code))
    {
        reply->set_error_code(proto::FILE_ERROR_ACCESS_DENIED);
        return reply;
    }

    reply->set_error_code(proto::FILE_ERROR_SUCCESS);
    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doDownloadRequest(
    const proto::DownloadRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    packetizer_ = FilePacketizer::create(std::filesystem::u8path(request.path()));
    if (!packetizer_)
        reply->set_error_code(proto::FILE_ERROR_FILE_OPEN_ERROR);
    else
        reply->set_error_code(proto::FILE_ERROR_SUCCESS);

    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doUploadRequest(
    const proto::UploadRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    std::filesystem::path file_path = std::filesystem::u8path(request.path());

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

    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doPacketRequest(
    const proto::FilePacketRequest& request)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    if (!packetizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::FILE_ERROR_UNKNOWN);
        LOG(LS_WARNING) << "Unexpected file packet request";
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

    return reply;
}

std::unique_ptr<proto::FileReply> FileWorker::Impl::doPacket(const proto::FilePacket& packet)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    if (!depacketizer_)
    {
        // Set the unknown status of the request. The connection will be closed.
        reply->set_error_code(proto::FILE_ERROR_UNKNOWN);
        LOG(LS_WARNING) << "Unexpected file packet";
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

    return reply;
}

FileWorker::FileWorker(std::shared_ptr<base::TaskRunner> task_runner)
    : impl_(std::make_shared<Impl>(std::move(task_runner)))
{
    // Nothing
}

FileWorker::~FileWorker() = default;

void FileWorker::doTask(std::shared_ptr<FileTask> task)
{
    impl_->doTask(std::move(task));
}

} // namespace common
