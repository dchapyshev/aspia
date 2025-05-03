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

#include "common/file_task_factory.h"

#include "base/logging.h"
#include "proto/file_transfer.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileTaskFactory::FileTaskFactory(FileTask::Target target, QObject* parent)
    : QObject(parent),
      target_(target)
{
    DCHECK(target_ == FileTask::Target::LOCAL || target_ == FileTask::Target::REMOTE);
}

//--------------------------------------------------------------------------------------------------
FileTaskFactory::~FileTaskFactory() = default;

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::driveList()
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_drive_list_request()->set_dummy(1);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::fileList(const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_file_list_request()->set_path(path);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::createDirectory(const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_create_directory_request()->set_path(path);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::rename(
    const std::string& old_name, const std::string& new_name)
{
    auto request = std::make_unique<proto::FileRequest>();

    proto::RenameRequest* rename_request = request->mutable_rename_request();
    rename_request->set_old_name(old_name);
    rename_request->set_new_name(new_name);

    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::remove(const std::string& path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_remove_request()->set_path(path);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::download(const std::string& file_path)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_download_request()->set_path(file_path);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::upload(const std::string& file_path, bool overwrite)
{
    auto request = std::make_unique<proto::FileRequest>();

    proto::UploadRequest* upload_request = request->mutable_upload_request();
    upload_request->set_path(file_path);
    upload_request->set_overwrite(overwrite);

    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::packetRequest(quint32 flags)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_packet_request()->set_flags(flags);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::packet(const proto::FilePacket& packet)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->mutable_packet()->CopyFrom(packet);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::packet(std::unique_ptr<proto::FilePacket> packet)
{
    auto request = std::make_unique<proto::FileRequest>();
    request->set_allocated_packet(packet.release());
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<FileTask> FileTaskFactory::makeTask(std::unique_ptr<proto::FileRequest> request)
{
    return base::make_local_shared<FileTask>(this, std::move(request), target_);
}

} // namespace common
