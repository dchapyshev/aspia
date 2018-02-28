//
// PROJECT:         Aspia
// FILE:            client/file_transfer_downloader.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H

#include "base/threading/thread.h"
#include "client/file_transfer.h"
#include "protocol/file_depacketizer.h"

namespace aspia {

class FileTransferDownloader
    : public FileTransfer,
      private Thread::Delegate
{
public:
    FileTransferDownloader(std::shared_ptr<FileRequestSenderProxy> local_sender,
                           std::shared_ptr<FileRequestSenderProxy> remote_sender,
                           FileTransfer::Delegate* delegate);
    ~FileTransferDownloader();

    void Start(const std::experimental::filesystem::path& source_path,
               const std::experimental::filesystem::path& target_path,
               const FileTaskQueueBuilder::FileList& file_list) final;

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnTaskQueueBuilded(FileTaskQueue& task_queue,
                            int64_t task_object_size,
                            int64_t task_object_count);
    void RunTask(const FileTask& task);
    void RunNextTask();

    // FileReplyReceiver implementation.
    void OnCreateDirectoryReply(const std::experimental::filesystem::path& path,
                                proto::file_transfer::Status status) final;
    void OnFileDownloadReply(const std::experimental::filesystem::path& file_path,
                             proto::file_transfer::Status status) final;
    void OnFilePacketReceived(std::shared_ptr<proto::file_transfer::FilePacket> file_packet,
                              proto::file_transfer::Status status) final;
    void CreateDepacketizer(const std::experimental::filesystem::path& file_path, bool overwrite);

    void OnUnableToCreateDirectoryAction(FileAction action);
    void OnUnableToCreateFileAction(FileAction action);
    void OnUnableToOpenFileAction(FileAction action);
    void OnUnableToReadFileAction(FileAction action);
    void OnUnableToWriteFileAction(FileAction action);

    Thread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    FileTaskQueueBuilder task_queue_builder_;
    FileTaskQueue task_queue_;
    std::unique_ptr<FileDepacketizer> file_depacketizer_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDownloader);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
