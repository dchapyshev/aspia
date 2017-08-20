//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_downloader.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_task_queue_builder.h"
#include "client/file_transfer.h"
#include "protocol/file_depacketizer.h"

namespace aspia {

class FileTransferDownloader
    : public FileTransfer,
      private MessageLoopThread::Delegate
{
public:
    FileTransferDownloader(std::shared_ptr<FileRequestSenderProxy> local_sender,
                           std::shared_ptr<FileRequestSenderProxy> remote_sender,
                           FileTransfer::Delegate* delegate);
    ~FileTransferDownloader();

    void Start(const FilePath& source_path,
               const FilePath& target_path,
               const FileList& file_list) final;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnTaskQueueBuilded(FileTaskQueue& task_queue,
                            int64_t task_object_size,
                            int64_t task_object_count);
    void RunTask(const FileTask& task);
    void RunNextTask();

    // FileReplyReceiver implementation.
    void OnCreateDirectoryReply(const FilePath& path, proto::RequestStatus status) final;
    void OnFileDownloadReply(const FilePath& file_path, proto::RequestStatus status) final;
    void OnFilePacketReceived(std::shared_ptr<proto::FilePacket> file_packet,
                              proto::RequestStatus status) final;
    void CreateDepacketizer(const FilePath& file_path, bool overwrite);

    void OnUnableToCreateDirectoryAction(Action action);
    void OnUnableToCreateFileAction(Action action);
    void OnUnableToOpenFileAction(Action action);
    void OnUnableToReadFileAction(Action action);
    void OnUnableToWriteFileAction(Action action);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    FileTaskQueueBuilder task_queue_builder_;
    FileTaskQueue task_queue_;
    std::unique_ptr<FileDepacketizer> file_depacketizer_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDownloader);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
