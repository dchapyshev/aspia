//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request_sender_proxy.h"

namespace aspia {

FileRequestSenderProxy::FileRequestSenderProxy(FileRequestSender* sender) :
    sender_(sender)
{
    // Nothing
}

void FileRequestSenderProxy::WillDestroyCurrentRequestSender()
{
    std::lock_guard<std::mutex> lock(sender_lock_);
    sender_ = nullptr;
}

bool FileRequestSenderProxy::SendDriveListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendDriveListRequest(receiver);
    return true;
}

bool FileRequestSenderProxy::SendFileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileListRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendCreateDirectoryRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendDirectorySizeRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendRemoveRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendRemoveRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendRenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& old_name,
    const FilePath& new_name)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendRenameRequest(receiver, old_name, new_name);
    return true;
}

bool FileRequestSenderProxy::SendFileUploadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& file_path,
    FileRequestSender::Overwrite overwrite)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileUploadRequest(receiver, file_path, overwrite);
    return true;
}

bool FileRequestSenderProxy::SendFileUploadDataRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    std::unique_ptr<proto::FilePacket> file_packet)
{
    std::lock_guard<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileUploadDataRequest(receiver, std::move(file_packet));
    return true;
}

} // namespace aspia
