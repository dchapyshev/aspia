//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver_proxy.h"

namespace aspia {

FileReplyReceiverProxy::FileReplyReceiverProxy(FileReplyReceiver* receiver) :
    receiver_(receiver)
{
    // Nothing
}

void FileReplyReceiverProxy::WillDestroyCurrentReplyReceiver()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);
    receiver_ = nullptr;
}

bool FileReplyReceiverProxy::OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListRequestReply(std::move(drive_list));
    return true;
}

bool FileReplyReceiverProxy::OnDriveListRequestFailure(proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListRequestFailure(status);
    return true;
}

bool FileReplyReceiverProxy::OnFileListRequestReply(std::unique_ptr<proto::FileList> file_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListRequestReply(std::move(file_list));
    return true;
}

bool FileReplyReceiverProxy::OnFileListRequestFailure(proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListRequestFailure(status);
    return true;
}

bool FileReplyReceiverProxy::OnCreateDirectoryRequestReply(proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnCreateDirectoryRequestReply(status);
    return true;
}

bool FileReplyReceiverProxy::OnRemoveRequestReply(proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRemoveRequestReply(status);
    return true;
}

bool FileReplyReceiverProxy::OnRenameRequestReply(proto::RequestStatus status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRenameRequestReply(status);
    return true;
}

} // namespace aspia
