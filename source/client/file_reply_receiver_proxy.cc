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

bool FileReplyReceiverProxy::OnDriveListRequestReply(
    std::unique_ptr<proto::DriveList> drive_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListRequestReply(std::move(drive_list));
    return true;
}

bool FileReplyReceiverProxy::OnDriveListRequestFailure(
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListRequestFailure(status);
    return true;
}

bool FileReplyReceiverProxy::OnFileListRequestReply(
    const FilePath& path,
    std::unique_ptr<proto::FileList> file_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListRequestReply(path, std::move(file_list));
    return true;
}

bool FileReplyReceiverProxy::OnFileListRequestFailure(
    const FilePath& path,
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListRequestFailure(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnDirectorySizeRequestReply(const FilePath& path,
                                                         uint64_t size)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDirectorySizeRequestReply(path, size);
    return true;
}

bool FileReplyReceiverProxy::OnDirectorySizeRequestFailure(
    const FilePath& path,
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDirectorySizeRequestFailure(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnCreateDirectoryRequestReply(
    const FilePath& path,
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnCreateDirectoryRequestReply(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnRemoveRequestReply(const FilePath& path,
                                                  proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRemoveRequestReply(path, status);
    return true;
}

bool FileReplyReceiverProxy::OnRenameRequestReply(const FilePath& old_name,
                                                  const FilePath& new_name,
                                                  proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRenameRequestReply(old_name, new_name, status);
    return true;
}

bool FileReplyReceiverProxy::OnFileUploadRequestReply(
    const FilePath& file_path,
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileUploadRequestReply(file_path, status);
    return true;
}

bool FileReplyReceiverProxy::OnFileUploadDataRequestReply(
    std::unique_ptr<proto::FilePacket> file_packet,
    proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileUploadDataRequestReply(std::move(file_packet), status);
    return true;
}

} // namespace aspia
