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
FileTask FileTaskFactory::driveList()
{
    proto::FileRequest request;
    request.mutable_drive_list_request()->set_dummy(1);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::fileList(const QString& path)
{
    proto::FileRequest request;
    request.mutable_file_list_request()->set_path(path.toStdString());
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::createDirectory(const QString& path)
{
    proto::FileRequest request;
    request.mutable_create_directory_request()->set_path(path.toStdString());
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::rename(const QString& old_name, const QString& new_name)
{
    proto::FileRequest request;

    proto::RenameRequest* rename_request = request.mutable_rename_request();
    rename_request->set_old_name(old_name.toStdString());
    rename_request->set_new_name(new_name.toStdString());

    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::remove(const QString& path)
{
    proto::FileRequest request;
    request.mutable_remove_request()->set_path(path.toStdString());
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::download(const QString& file_path)
{
    proto::FileRequest request;
    request.mutable_download_request()->set_path(file_path.toStdString());
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::upload(const QString& file_path, bool overwrite)
{
    proto::FileRequest request;

    proto::UploadRequest* upload_request = request.mutable_upload_request();
    upload_request->set_path(file_path.toStdString());
    upload_request->set_overwrite(overwrite);

    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::packetRequest(quint32 flags)
{
    proto::FileRequest request;
    request.mutable_packet_request()->set_flags(flags);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::packet(const proto::FilePacket& packet)
{
    proto::FileRequest request;
    request.mutable_packet()->CopyFrom(packet);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::packet(proto::FilePacket&& packet)
{
    proto::FileRequest request;
    *request.mutable_packet() = std::move(packet);
    return makeTask(std::move(request));
}

//--------------------------------------------------------------------------------------------------
FileTask FileTaskFactory::makeTask(proto::FileRequest&& request)
{
    return FileTask(this, std::move(request), target_);
}

} // namespace common
