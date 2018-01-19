//
// PROJECT:         Aspia
// FILE:            protocol/file_request_sender_local.h
// LICENSE:         Mozilla Public License Version 2.0
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
    void SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver) override;

    void SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                             const std::experimental::filesystem::path& path) override;

    void SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                    const std::experimental::filesystem::path& path) override;

    void SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                  const std::experimental::filesystem::path& path) override;

    void SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const std::experimental::filesystem::path& path) override;

    void SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                           const std::experimental::filesystem::path& old_name,
                           const std::experimental::filesystem::path& new_name) override;

    void SendFileUploadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                               const std::experimental::filesystem::path& file_path,
                               Overwrite overwrite) override;

    void SendFileDownloadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                 const std::experimental::filesystem::path& file_path) override;

    void SendFilePacket(std::shared_ptr<FileReplyReceiverProxy> receiver,
                        std::unique_ptr<proto::file_transfer::FilePacket> file_packet) override;

    void SendFilePacketRequest(std::shared_ptr<FileReplyReceiverProxy> receiver) override;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void DriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver);

    void FileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                         const std::experimental::filesystem::path& path);

    void CreateDirectoryRequest(
        std::shared_ptr<FileReplyReceiverProxy> receiver,
        const std::experimental::filesystem::path& path);

    void DirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                              const std::experimental::filesystem::path& path);

    void RemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                       const std::experimental::filesystem::path& path);

    void RenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                       const std::experimental::filesystem::path& old_name,
                       const std::experimental::filesystem::path& new_name);

    MessageLoopThread worker_thread_;
    std::shared_ptr<MessageLoopProxy> worker_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestSenderLocal);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_SENDER_LOCAL_H
