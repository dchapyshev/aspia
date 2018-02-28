//
// PROJECT:         Aspia
// FILE:            client/file_request_sender_proxy.h
// LICENSE:         GNU Lesser General Public License 2.1
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
                             const std::experimental::filesystem::path& path);

    bool SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                    const std::experimental::filesystem::path& path);

    bool SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                  const std::experimental::filesystem::path& path);

    bool SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const std::experimental::filesystem::path& path);

    bool SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const std::experimental::filesystem::path& old_name,
                           const std::experimental::filesystem::path& new_name);

    bool SendFileUploadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                               const std::experimental::filesystem::path& file_path,
                               FileRequestSender::Overwrite overwrite);

    bool SendFileDownloadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                 const std::experimental::filesystem::path& file_path);

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
