//
// PROJECT:         Aspia
// FILE:            client/file_request.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request.h"

namespace aspia {

// static
proto::file_transfer::Request FileRequest::driveListRequest()
{
    proto::file_transfer::Request request;
    request.mutable_drive_list_request()->set_dummy(1);
    return request;
}

// static
proto::file_transfer::Request FileRequest::fileListRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_file_list_request()->set_path(path.toUtf8());
    return request;
}

// static
proto::file_transfer::Request FileRequest::createDirectoryRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_create_directory_request()->set_path(path.toUtf8());
    return request;
}

// static
proto::file_transfer::Request FileRequest::renameRequest(
    const QString& old_name, const QString& new_name)
{
    proto::file_transfer::Request request;
    request.mutable_rename_request()->set_old_name(old_name.toUtf8());
    request.mutable_rename_request()->set_new_name(new_name.toUtf8());
    return request;
}

// static
proto::file_transfer::Request FileRequest::removeRequest(const QString& path)
{
    proto::file_transfer::Request request;
    request.mutable_remove_request()->set_path(path.toUtf8());
    return request;
}

// static
proto::file_transfer::Request FileRequest::downloadRequest(const QString& file_path)
{
    proto::file_transfer::Request request;
    request.mutable_download_request()->set_file_path(file_path.toUtf8());
    return request;
}

// static
proto::file_transfer::Request FileRequest::uploadRequest(const QString& file_path, bool overwrite)
{
    proto::file_transfer::Request request;
    request.mutable_upload_request()->set_file_path(file_path.toUtf8());
    request.mutable_upload_request()->set_overwrite(overwrite);
    return request;
}

// static
proto::file_transfer::Request FileRequest::packetRequest()
{
    proto::file_transfer::Request request;
    request.mutable_packet_request()->set_dummy(1);
    return request;
}

// static
proto::file_transfer::Request FileRequest::packet(const proto::file_transfer::Packet& packet)
{
    proto::file_transfer::Request request;
    request.mutable_packet()->CopyFrom(packet);
    return request;
}

} // namespace aspia
