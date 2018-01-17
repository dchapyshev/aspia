//
// PROJECT:         Aspia
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

void FileReplyReceiver::OnDriveListReply(
    std::shared_ptr<proto::file_transfer::DriveList> /* drive_list */,
    proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: drive list";
}

void FileReplyReceiver::OnFileListReply(
    const FilePath& /* path */,
    std::shared_ptr<proto::file_transfer::FileList> /* file_list */,
    proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: file list";
}

void FileReplyReceiver::OnDirectorySizeReply(const FilePath& /* path */,
                                             uint64_t /* size */,
                                             proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: directory size";
}

void FileReplyReceiver::OnCreateDirectoryReply(
    const FilePath& /* path */, proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: create directory";
}

void FileReplyReceiver::OnRemoveReply(
    const FilePath& /* path */, proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: remove";
}

void FileReplyReceiver::OnRenameReply(const FilePath& /* old_name */,
                                      const FilePath& /* new_name */,
                                      proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: rename";
}

void FileReplyReceiver::OnFileUploadReply(
    const FilePath& /* file_path */, proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: file upload";
}

void FileReplyReceiver::OnFileDownloadReply(
    const FilePath& /* file_path */, proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: file download";
}

void FileReplyReceiver::OnFilePacketSended(
    uint32_t /* flags */, proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: file packet sended";
}

void FileReplyReceiver::OnFilePacketReceived(
    std::shared_ptr<proto::file_transfer::FilePacket> /* file_packet */,
    proto::file_transfer::Status /* status */)
{
    DLOG(WARNING) << "Unhandled reply: file packet received";
}

} // namespace aspia
