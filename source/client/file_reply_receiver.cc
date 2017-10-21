//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver.h"
#include "client/file_reply_receiver_proxy.h"
#include "base/logging.h"

namespace aspia {

FileReplyReceiver::FileReplyReceiver()
{
    receiver_proxy_.reset(new FileReplyReceiverProxy(this));
}

FileReplyReceiver::~FileReplyReceiver()
{
    receiver_proxy_->WillDestroyCurrentReplyReceiver();
    receiver_proxy_ = nullptr;
}

void FileReplyReceiver::OnDriveListReply(std::shared_ptr<proto::DriveList> drive_list,
                                         proto::RequestStatus status)
{
    UNUSED_PARAMETER(drive_list);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: drive list";
}

void FileReplyReceiver::OnFileListReply(const FilePath& path,
                                        std::shared_ptr<proto::FileList> file_list,
                                        proto::RequestStatus status)
{
    UNUSED_PARAMETER(path);
    UNUSED_PARAMETER(file_list);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: file list";
}

void FileReplyReceiver::OnDirectorySizeReply(const FilePath& path,
                                  uint64_t size,
                                  proto::RequestStatus status)
{
    UNUSED_PARAMETER(path);
    UNUSED_PARAMETER(size);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: directory size";
}

void FileReplyReceiver::OnCreateDirectoryReply(const FilePath& path, proto::RequestStatus status)
{
    UNUSED_PARAMETER(path);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: create directory";
}

void FileReplyReceiver::OnRemoveReply(const FilePath& path, proto::RequestStatus status)
{
    UNUSED_PARAMETER(path);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: remove";
}

void FileReplyReceiver::OnRenameReply(const FilePath& old_name,
                                      const FilePath& new_name,
                                      proto::RequestStatus status)
{
    UNUSED_PARAMETER(old_name);
    UNUSED_PARAMETER(new_name);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: rename";
}

void FileReplyReceiver::OnFileUploadReply(const FilePath& file_path, proto::RequestStatus status)
{
    UNUSED_PARAMETER(file_path);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: file upload";
}

void FileReplyReceiver::OnFileDownloadReply(const FilePath& file_path, proto::RequestStatus status)
{
    UNUSED_PARAMETER(file_path);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: file download";
}

void FileReplyReceiver::OnFilePacketSended(uint32_t flags, proto::RequestStatus status)
{
    UNUSED_PARAMETER(flags);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: file packet sended";
}

void FileReplyReceiver::OnFilePacketReceived(std::shared_ptr<proto::FilePacket> file_packet,
                                             proto::RequestStatus status)
{
    UNUSED_PARAMETER(file_packet);
    UNUSED_PARAMETER(status);

    DLOG(WARNING) << "Unhandled reply: file packet received";
}

} // namespace aspia
