//
// PROJECT:         Aspia
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

namespace aspia {

class FileRequestSenderRemote : public FileRequestSender
{
public:
    FileRequestSenderRemote(std::shared_ptr<NetworkChannelProxy> channel_proxy);
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

    void SendFileUploadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                               const FilePath& file_path,
                               Overwrite overwrite) override;

    void SendFileDownloadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                 const FilePath& file_path) override;

    void SendFilePacket(std::shared_ptr<FileReplyReceiverProxy> receiver,
                        std::unique_ptr<proto::file_transfer::FilePacket> file_packet) override;

    void SendFilePacketRequest(std::shared_ptr<FileReplyReceiverProxy> receiver) override;

private:
    void SendRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                     proto::file_transfer::ClientToHost&& request);

    void OnRequestSended();
    void OnReplyReceived(const IOBuffer& buffer);
    bool ProcessNextReply(proto::file_transfer::HostToClient& reply);

    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    proto::file_transfer::ClientToHost last_request_;
    std::shared_ptr<FileReplyReceiverProxy> last_receiver_;
    std::mutex send_lock_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderRemote);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_REMOTE_H
