//
// PROJECT:         Aspia
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
    std::scoped_lock<std::mutex> lock(sender_lock_);
    sender_ = nullptr;
}

bool FileRequestSenderProxy::SendDriveListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendDriveListRequest(receiver);
    return true;
}

bool FileRequestSenderProxy::SendFileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileListRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendCreateDirectoryRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendDirectorySizeRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendRemoveRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendRemoveRequest(receiver, path);
    return true;
}

bool FileRequestSenderProxy::SendRenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendRenameRequest(receiver, old_name, new_name);
    return true;
}

bool FileRequestSenderProxy::SendFileUploadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& file_path,
    FileRequestSender::Overwrite overwrite)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileUploadRequest(receiver, file_path, overwrite);
    return true;
}

bool FileRequestSenderProxy::SendFileDownloadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& file_path)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFileDownloadRequest(receiver, file_path);
    return true;
}

bool FileRequestSenderProxy::SendFilePacket(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    std::unique_ptr<proto::file_transfer::FilePacket> file_packet)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFilePacket(receiver, std::move(file_packet));
    return true;
}

bool FileRequestSenderProxy::SendFilePacketRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    std::scoped_lock<std::mutex> lock(sender_lock_);

    if (!sender_)
        return false;

    sender_->SendFilePacketRequest(receiver);
    return true;
}

} // namespace aspia
