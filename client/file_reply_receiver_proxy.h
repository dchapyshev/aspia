//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H

#include "base/macros.h"
#include "client/file_reply_receiver.h"

#include <mutex>

namespace aspia {

class FileReplyReceiverProxy
{
public:
    bool OnLastRequestFailed(proto::Status status);
    bool OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list);
    bool OnFileListReply(std::unique_ptr<proto::FileList> file_list);
    bool OnCreateDirectoryReply();
    bool OnRemoveReply();
    bool OnRenameReply();

private:
    friend class FileReplyReceiver;

    explicit FileReplyReceiverProxy(FileReplyReceiver* receiver);

    // Called directly by FileReplyReceiver::~FileReplyReceiver.
    void WillDestroyCurrentClientSession();

    FileReplyReceiver* receiver_;
    std::mutex receiver_lock_;

    DISALLOW_COPY_AND_ASSIGN(FileReplyReceiverProxy);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H
