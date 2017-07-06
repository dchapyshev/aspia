//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_local.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_SENDER_LOCAL_H
#define _ASPIA_CLIENT__FILE_REQUEST_SENDER_LOCAL_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_request_sender.h"
#include "client/file_reply_receiver.h"

namespace aspia {

class FileRequestSenderLocal :
    public FileRequestSender,
    private MessageLoopThread::Delegate
{
public:
    FileRequestSenderLocal();
    ~FileRequestSenderLocal();

    // FileRequestSender implementation.
    void SendDriveListRequest(FileReplyReceiver* receiver) override;
    void SendFileListRequest(FileReplyReceiver* receiver, const FilePath& path) override;
    void SendCreateDirectoryRequest(FileReplyReceiver* receiver, const FilePath& path) override;
    void SendDirectorySizeRequest(FileReplyReceiver* receiver, const FilePath& path) override;
    void SendRemoveRequest(FileReplyReceiver* receiver, const FilePath& path) override;
    void SendRenameRequest(FileReplyReceiver* receiver,
                           const FilePath& old_name,
                           const FilePath& new_name) override;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    MessageLoopThread worker_thread_;
    std::shared_ptr<MessageLoopProxy> worker_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderLocal);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_LOCAL_H
