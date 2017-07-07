//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_remote.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_SENDER_REMOTE_H
#define _ASPIA_CLIENT__FILE_REQUEST_SENDER_REMOTE_H

#include "base/macros.h"
#include "client/client_session.h"
#include "client/file_request_sender.h"
#include "client/file_reply_receiver.h"
#include "protocol/io_buffer.h"

#include <functional>
#include <mutex>
#include <queue>

namespace aspia {

class FileRequestSenderRemote : public FileRequestSender
{
public:
    FileRequestSenderRemote(ClientSession::Delegate* session);
    ~FileRequestSenderRemote() = default;

    void SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver) override;

    void SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                             const FilePath& path) override;

    void SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                    const FilePath& path) override;

    void SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                  const FilePath& path) override;

    void SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const FilePath& path) override;

    void SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const FilePath& old_name,
                           const FilePath& new_name) override;

    bool ReadIncommingMessage(const IOBuffer& buffer);

private:
    void FileRequestSenderRemote::SendRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                              const proto::file_transfer::ClientToHost& request);

    ClientSession::Delegate* session_;

    std::queue<std::shared_ptr<FileReplyReceiverProxy>> receiver_queue_;
    std::mutex receiver_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderRemote);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_REMOTE_H
