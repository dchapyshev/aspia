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

#include "common/file_request.h"

namespace aspia {

FileRequest::FileRequest(proto::file_transfer::Request&& request)
    : request_(std::move(request))
{
    // Nothing
}

void FileRequest::sendReply(const proto::file_transfer::Reply& reply)
{
    emit replyReady(request_, reply);
}

// static
FileRequest* FileRequest::driveListRequest()
{
    proto::file_transfer::Request request;
    request.mutable_drive_list_request()->set_dummy(1);
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::fileListRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_file_list_request()->set_path(path.toStdString());
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::createDirectoryRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_create_directory_request()->set_path(path.toStdString());
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::renameRequest(const QString& old_name, const QString& new_name)
{
    proto::file_transfer::Request request;
    request.mutable_rename_request()->set_old_name(old_name.toStdString());
    request.mutable_rename_request()->set_new_name(new_name.toStdString());
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::removeRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_remove_request()->set_path(path.toStdString());
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::downloadRequest(const QString& file_path)
{
    proto::file_transfer::Request request;
    request.mutable_download_request()->set_path(file_path.toStdString());
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::uploadRequest(const QString& file_path, bool overwrite)
{
    proto::file_transfer::Request request;
    request.mutable_upload_request()->set_path(file_path.toStdString());
    request.mutable_upload_request()->set_overwrite(overwrite);
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::packetRequest(uint32_t flags)
{
    proto::file_transfer::Request request;
    request.mutable_packet_request()->set_flags(flags);
    return new FileRequest(std::move(request));
}

// static
FileRequest* FileRequest::packet(const proto::file_transfer::Packet& packet)
{
    proto::file_transfer::Request request;
    request.mutable_packet()->CopyFrom(packet);
    return new FileRequest(std::move(request));
}

} // namespace aspia
