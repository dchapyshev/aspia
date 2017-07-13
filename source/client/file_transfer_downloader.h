//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_downloader.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H

#include "client/file_transfer.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileTransferDownloader : public FileTransfer
{
public:
    FileTransferDownloader(std::shared_ptr<FileRequestSenderProxy> sender,
                           Delegate* delegate);

    ~FileTransferDownloader() = default;

    void Start(const FilePath& source_path,
               const FilePath& target_path,
               const FileList& file_list) final;

private:
    // FileReplyReceiver implementation.
    void OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list) final;

    void OnDriveListRequestFailure(proto::RequestStatus status) final;

    void OnFileListRequestReply(const FilePath& path,
                                std::unique_ptr<proto::FileList> file_list) final;

    void OnFileListRequestFailure(const FilePath& path,
                                  proto::RequestStatus status) final;

    void OnDirectorySizeRequestReply(const FilePath& path,
                                     uint64_t size) final;

    void OnDirectorySizeRequestFailure(const FilePath& path,
                                       proto::RequestStatus status) final;

    void OnCreateDirectoryRequestReply(const FilePath& path,
                                       proto::RequestStatus status) final;

    void OnRemoveRequestReply(const FilePath& path,
                              proto::RequestStatus status) final;

    void OnRenameRequestReply(const FilePath& old_name,
                              const FilePath& new_name,
                              proto::RequestStatus status) final;

    void OnFileUploadRequestReply(const FilePath& file_path,
                                  proto::RequestStatus status) final;

    void OnFileUploadDataRequestReply(
        std::unique_ptr<proto::FilePacket> file_packet,
        proto::RequestStatus status) final;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDownloader);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
