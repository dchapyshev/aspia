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

    bool ProcessNextReply(std::unique_ptr<proto::file_transfer::HostToClient> reply);

    using Receiver = std::shared_ptr<FileReplyReceiverProxy>;
    using Request = std::unique_ptr<proto::file_transfer::ClientToHost>;

    void Add(Request request, Receiver receiver);

private:
    std::queue<std::pair<Request, Receiver>> queue_;
    std::mutex queue_lock_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_QUEUE_H
