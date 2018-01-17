//
// PROJECT:         Aspia
// FILE:            protocol/file_request_sender_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_SENDER_PROXY_H
#define _ASPIA_CLIENT__FILE_REQUEST_SENDER_PROXY_H

#include "base/macros.h"
#include "client/file_request_sender.h"

#include <mutex>

namespace aspia {

class FileRequestSenderProxy
{
public:
    bool SendDriveListRequest(
        std::shared_ptr<FileReplyReceiverProxy> receiver);

    bool SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                             const FilePath& path);

    bool SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                    const FilePath& path);

    bool SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                  const FilePath& path);

    bool SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const FilePath& path);

    bool SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const FilePath& old_name,
                           const FilePath& new_name);

    bool SendFileUploadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                               const FilePath& file_path,
                               FileRequestSender::Overwrite overwrite);

    bool SendFileDownloadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                 const FilePath& file_path);

    bool SendFilePacket(std::shared_ptr<FileReplyReceiverProxy> receiver,
                        std::unique_ptr<proto::file_transfer::FilePacket> file_packet);

    bool SendFilePacketRequest(std::shared_ptr<FileReplyReceiverProxy> receiver);

private:
    friend class FileRequestSender;

    explicit FileRequestSenderProxy(FileRequestSender* sender);

    // Called directly by FileRequestSender::~FileRequestSender.
    void WillDestroyCurrentRequestSender();

    FileRequestSender* sender_;
    std::mutex sender_lock_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderProxy);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_PROXY_H
