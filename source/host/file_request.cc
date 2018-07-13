//
// PROJECT:         Aspia
// FILE:            host/file_request.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_request.h"

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
FileRequest* FileRequest::packetRequest()
{
    proto::file_transfer::Request request;
    request.mutable_packet_request()->set_dummy(1);
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
