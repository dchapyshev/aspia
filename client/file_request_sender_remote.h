//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_remote.h
// LICENSE:         See top-level directory
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

namespace aspia {

class FileRequestSenderRemote : public FileRequestSender
{
public:
    FileRequestSenderRemote(ClientSession::Delegate* session);
    ~FileRequestSenderRemote() = default;

    void SendDriveListRequest(FileReplyReceiver* receiver) override;

    void SendFileListRequest(FileReplyReceiver* receiver, const FilePath& path) override;

    void SendCreateDirectoryRequest(FileReplyReceiver* receiver, const FilePath& path) override;

    void SendDirectorySizeRequest(FileReplyReceiver* receiver, const FilePath& path) override;

    void SendRemoveRequest(FileReplyReceiver* receiver, const FilePath& path) override;

    void SendRenameRequest(FileReplyReceiver* receiver,
                           const FilePath& old_name,
                           const FilePath& new_name) override;

    bool ReadIncommingMessage(const IOBuffer& buffer);

private:
    void FileRequestSenderRemote::SendRequest(const proto::file_transfer::ClientToHost& request);

    ClientSession::Delegate* session_;
    FileReplyReceiver* receiver_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderRemote);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_REMOTE_H
