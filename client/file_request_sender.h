//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_SENDER_H
#define _ASPIA_CLIENT__FILE_REQUEST_SENDER_H

#include "base/files/file_path.h"
#include "client/file_reply_receiver.h"

namespace aspia {

class FileRequestSender
{
public:
    virtual ~FileRequestSender() = default;

    virtual void SendDriveListRequest(FileReplyReceiver* receiver) = 0;

    virtual void SendFileListRequest(FileReplyReceiver* receiver, const FilePath& path) = 0;

    virtual void SendCreateDirectoryRequest(FileReplyReceiver* receiver, const FilePath& path) = 0;

    virtual void SendDirectorySizeRequest(FileReplyReceiver* receiver, const FilePath& path) = 0;

    virtual void SendRemoveRequest(FileReplyReceiver* receiver, const FilePath& path) = 0;

    virtual void SendRenameRequest(FileReplyReceiver* receiver,
                                   const FilePath& old_name,
                                   const FilePath& new_name) = 0;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_H
