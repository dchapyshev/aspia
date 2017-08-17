//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_uploader.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_transfer.h"
#include "client/file_task.h"
#include "proto/file_transfer_session.pb.h"
#include "protocol/file_packetizer.h"

namespace aspia {

class FileTransferUploader
    : public FileTransfer,
      private MessageLoopThread::Delegate
{
public:
    FileTransferUploader(std::shared_ptr<FileRequestSenderProxy> sender,
                         FileTransfer::Delegate* delegate);
    ~FileTransferUploader();

    // FileTransfer implementation.
    void Start(const FilePath& source_path,
               const FilePath& target_path,
               const FileList& file_list) final;
    void OnUnableToCreateDirectoryAction(Action action) final;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    uint64_t BuildTaskListForDirectoryContent(const FilePath& source_path,
                                              const FilePath& target_path);

    void RunTask(const FileTask& task, FileRequestSender::Overwrite overwrite);
    void RunNextTask();

    // FileReplyReceiver implementation.
    void OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list,
                          proto::RequestStatus status) final;
    void OnFileListReply(const FilePath& path,
                         std::unique_ptr<proto::FileList> file_list,
                         proto::RequestStatus status) final;
    void OnDirectorySizeReply(const FilePath& path,
                              uint64_t size,
                              proto::RequestStatus status) final;
    void OnCreateDirectoryReply(const FilePath& path, proto::RequestStatus status) final;
    void OnRemoveReply(const FilePath& path, proto::RequestStatus status) final;
    void OnRenameReply(const FilePath& old_name, const FilePath& new_name, proto::RequestStatus status) final;
    void OnFileUploadReply(const FilePath& file_path, proto::RequestStatus status) final;
    void OnFileDownloadReply(const FilePath& file_path, proto::RequestStatus status) final;
    void OnFilePacketSended(uint32_t flags, proto::RequestStatus status) final;
    void OnFilePacketReceived(std::unique_ptr<proto::FilePacket> file_packet,
                              proto::RequestStatus status) final;

    void OnUnableToCreateFileAction(Action action);
    void OnUnableToOpenFileAction(Action action);
    void OnUnableToReadFileAction(Action action);
    void OnUnableToWriteFileAction(Action action);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    FileTaskQueue task_queue_;
    std::unique_ptr<FilePacketizer> file_packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferUploader);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
