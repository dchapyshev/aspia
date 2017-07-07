//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_queue.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_QUEUE_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_QUEUE_H

#include "client/file_reply_receiver_proxy.h"

#include <queue>
#include <mutex>

namespace aspia {

class FileReplyReceiverQueue
{
public:
    FileReplyReceiverQueue() = default;
    ~FileReplyReceiverQueue() = default;

    std::shared_ptr<FileReplyReceiverProxy> NextReceiver();
    void AddReceiver(std::shared_ptr<FileReplyReceiverProxy> receiver);

private:
    std::queue<std::shared_ptr<FileReplyReceiverProxy>> queue_;
    std::mutex queue_lock_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_QUEUE_H
