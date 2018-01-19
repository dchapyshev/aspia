//
// PROJECT:         Aspia
// FILE:            client/file_transfer_uploader.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_transfer.h"
#include "proto/file_transfer_session.pb.h"
#include "protocol/file_packetizer.h"

namespace aspia {

class FileTransferUploader
    : public FileTransfer,
      private MessageLoopThread::Delegate
{
public:
    FileTransferUploader(std::shared_ptr<FileRequestSenderProxy> local_sender,
                         std::shared_ptr<FileRequestSenderProxy> remote_sender,
                         FileTransfer::Delegate* delegate);
    ~FileTransferUploader();

    // FileTransfer implementation.
    void Start(const std::experimental::filesystem::path& source_path,
               const std::experimental::filesystem::path& target_path,
               const FileTaskQueueBuilder::FileList& file_list) final;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() final;
    void OnAfterThreadRunning() final;

    void OnTaskQueueBuilded(FileTaskQueue& task_queue,
                            int64_t task_object_size,
                            int64_t task_object_count);
    void RunTask(const FileTask& task, FileRequestSender::Overwrite overwrite);
    void RunNextTask();

    // FileReplyReceiver implementation.
    void OnCreateDirectoryReply(const std::experimental::filesystem::path& path,
                                proto::file_transfer::Status status) final;
    void OnFileUploadReply(const std::experimental::filesystem::path& file_path,
                           proto::file_transfer::Status status) final;
    void OnFilePacketSended(uint32_t flags, proto::file_transfer::Status status) final;

    void OnUnableToCreateDirectoryAction(FileAction action);
    void OnUnableToCreateFileAction(FileAction action);
    void OnUnableToOpenFileAction(FileAction action);
    void OnUnableToReadFileAction(FileAction action);
    void OnUnableToWriteFileAction(FileAction action);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    FileTaskQueueBuilder task_queue_builder_;
    FileTaskQueue task_queue_;
    std::unique_ptr<FilePacketizer> file_packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferUploader);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
