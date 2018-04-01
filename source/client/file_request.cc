//
// PROJECT:         Aspia
// FILE:            client/file_request.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request.h"

namespace aspia {

FileRequest::FileRequest(QObject* sender,
                         proto::file_transfer::Request&& request,
                         const char* reply_slot)
    : sender_(sender),
      request_(std::move(request)),
      reply_slot_(reply_slot)
{
    connect(sender, &QObject::destroyed, this, &FileRequest::senderDestroyed);
}

bool FileRequest::sendReply(const proto::file_transfer::Reply& reply)
{
    if (!sender_ || !reply_slot_)
        return false;

    return QMetaObject::invokeMethod(sender_, reply_slot_,
                                     Q_ARG(const proto::file_transfer::Request&, request_),
                                     Q_ARG(const proto::file_transfer::Reply&, reply));
}

void FileRequest::senderDestroyed()
{
    sender_ = nullptr;
    reply_slot_ = nullptr;
}

// static
FileRequest* FileRequest::driveListRequest(QObject* sender, const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_drive_list_request()->set_dummy(1);
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::fileListRequest(QObject* sender,
                                          const QString& path,
                                          const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_file_list_request()->set_path(path.toStdString());
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::createDirectoryRequest(QObject* sender,
                                                 const QString& path,
                                                 const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_create_directory_request()->set_path(path.toStdString());
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::renameRequest(QObject* sender,
                                        const QString& old_name,
                                        const QString& new_name,
                                        const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_rename_request()->set_old_name(old_name.toStdString());
    request.mutable_rename_request()->set_new_name(new_name.toStdString());
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::removeRequest(QObject* sender,
                                        const QString& path,
                                        const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_remove_request()->set_path(path.toStdString());
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::downloadRequest(QObject* sender,
                                          const QString& file_path,
                                          const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_download_request()->set_path(file_path.toStdString());
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::uploadRequest(QObject* sender,
                                        const QString& file_path,
                                        bool overwrite,
                                        const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_upload_request()->set_path(file_path.toStdString());
    request.mutable_upload_request()->set_overwrite(overwrite);
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::packetRequest(QObject* sender, const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_packet_request()->set_dummy(1);
    return new FileRequest(sender, std::move(request), reply_slot);
}

// static
FileRequest* FileRequest::packet(QObject* sender,
                                 const proto::file_transfer::Packet& packet,
                                 const char* reply_slot)
{
    proto::file_transfer::Request request;
    request.mutable_packet()->CopyFrom(packet);
    return new FileRequest(sender, std::move(request), reply_slot);
}

} // namespace aspia
