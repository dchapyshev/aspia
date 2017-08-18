//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver_proxy.h"

namespace aspia {

FileReplyReceiverProxy::FileReplyReceiverProxy(FileReplyReceiver* receiver)
    : receiver_(receiver)
{
    // Nothing
}

void FileReplyReceiverProxy::WillDestroyCurrentReplyReceiver()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);
    receiver_ = nullptr;
}

bool FileReplyReceiverProxy::OnDriveListReply(std::shared_ptr<proto::DriveList> drive_list,
                                              proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListReply(std::move(drive_list), status);
    return true;
}

bool FileReplyReceiverProxy::OnFileListReply(const FilePath& path,
                                             std::shared_ptr<proto::FileList> file_list,
                                             proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListReply(path, std::move(file_list), status);
    return true;
}

bool FileReplyReceiverProxy::OnDirectorySizeReply(const FilePath& path,
                                                  uint64_t size,
                                                  proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDirectorySizeReply(path, size, status);
    return true;
}

bool FileReplyReceiverProxy::OnCreateDirectoryReply(const FilePath& path,
                                                    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnCreateDirectoryReply(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnRemoveReply(const FilePath& path, proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRemoveReply(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnRenameReply(const FilePath& old_name,
                                           const FilePath& new_name,
                                           proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRenameReply(old_name, new_name, status);
    return true;
}

bool FileReplyReceiverProxy::OnFileUploadReply(const FilePath& file_path,
                                               proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileUploadReply(file_path, status);
    return true;
}

bool FileReplyReceiverProxy::OnFileDownloadReply(const FilePath& file_path,
                                                 proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileDownloadReply(file_path, status);
    return true;
}

bool FileReplyReceiverProxy::OnFilePacketSended(uint32_t flags, proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFilePacketSended(flags, status);
    return true;
}

bool FileReplyReceiverProxy::OnFilePacketReceived(std::shared_ptr<proto::FilePacket> file_packet,
                                                  proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFilePacketReceived(std::move(file_packet), status);
    return true;
}

} // namespace aspia
