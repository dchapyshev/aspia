//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_H

#include "base/files/file_path.h"
#include "client/file_reply_receiver.h"
#include "client/file_request_sender_proxy.h"

namespace aspia {

class FileTransfer : public FileReplyReceiver
{
public:
    class Delegate
    {
    public:
        virtual void OnObjectSizeNotify(uint64_t size) = 0;

        virtual void OnTransferCompletionNotify() = 0;

        virtual void OnObjectTransferNotify(const FilePath& source_path,
                                            const FilePath& target_path) = 0;

        virtual void OnDirectoryOverwriteRequest(const FilePath& object_name,
                                                 const FilePath& path) = 0;

        virtual void OnFileOverwriteRequest(const FilePath& object_name,
                                            const FilePath& path) = 0;
    };

    FileTransfer(std::shared_ptr<FileRequestSenderProxy> sender, Delegate* delegate) :
        sender_(std::move(sender)),
        delegate_(delegate)
    {
        // Nothing
    }

    virtual ~FileTransfer() = default;

protected:
    std::shared_ptr<FileRequestSenderProxy> sender_;
    Delegate* delegate_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_H
