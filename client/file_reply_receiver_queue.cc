//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_queue.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver_queue.h"

namespace aspia {

std::shared_ptr<FileReplyReceiverProxy> FileReplyReceiverQueue::NextReceiver()
{
    std::shared_ptr<FileReplyReceiverProxy> receiver;

    {
        std::lock_guard<std::mutex> lock(queue_lock_);

        if (queue_.empty())
            return nullptr;

        receiver = std::move(queue_.front());
        queue_.pop();
    }

    return receiver;
}

void FileReplyReceiverQueue::AddReceiver(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    std::lock_guard<std::mutex> lock(queue_lock_);
    queue_.push(std::move(receiver));
}

} // namespace aspia
