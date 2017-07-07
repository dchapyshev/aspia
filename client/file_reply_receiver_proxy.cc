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

void FileReplyReceiverProxy::WillDestroyCurrentClientSession()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);
    receiver_ = nullptr;
}

bool FileReplyReceiverProxy::OnLastRequestFailed(proto::Status status)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnLastRequestFailed(status);
    return true;
}

bool FileReplyReceiverProxy::OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnDriveListReply(std::move(drive_list));
    return true;
}

bool FileReplyReceiverProxy::OnFileListReply(std::unique_ptr<proto::FileList> file_list)
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnFileListReply(std::move(file_list));
    return true;
}

bool FileReplyReceiverProxy::OnCreateDirectoryReply()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnCreateDirectoryReply();
    return true;
}

bool FileReplyReceiverProxy::OnRemoveReply()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRemoveReply();
    return true;
}

bool FileReplyReceiverProxy::OnRenameReply()
{
    std::lock_guard<std::mutex> lock(receiver_lock_);

    if (!receiver_)
        return false;

    receiver_->OnRenameReply();
    return true;
}

} // namespace aspia
