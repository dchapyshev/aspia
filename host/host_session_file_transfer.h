//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_file_transfer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
#define _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H

#include "host/host_session.h"
#include "base/message_loop/message_loop_thread.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class HostSessionFileTransfer :
    public HostSession,
    private MessageLoopThread::Delegate
{
public:
    HostSessionFileTransfer(HostSession::Delegate* delegate);
    ~HostSessionFileTransfer();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // HostSession implementation.
    void Send(const IOBuffer& buffer) override;

    void ReadDriveListRequestMessage(const proto::DriveListRequest& drive_list_request);
    void ReadDirectoryListRequestMessage(const proto::DirectoryListRequest& direcrory_list_request);
    void ReadFileRequestMessage(const proto::FileRequest& file_request);
    void ReadFileMessage(const proto::File& file);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionFileTransfer);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
