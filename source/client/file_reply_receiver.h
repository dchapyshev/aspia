//
// PROJECT:         Aspia
// FILE:            protocol/file_reply_receiver.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <experimental/filesystem>
#include <memory>

namespace aspia {

class FileReplyReceiverProxy;

class FileReplyReceiver
{
public:
    FileReplyReceiver();
    virtual ~FileReplyReceiver();

protected:
    std::shared_ptr<FileReplyReceiverProxy> This() const { return receiver_proxy_; }

    virtual void OnDriveListReply(std::shared_ptr<proto::file_transfer::DriveList> drive_list,
                                  proto::file_transfer::Status status);

    virtual void OnFileListReply(const std::experimental::filesystem::path& path,
                                 std::shared_ptr<proto::file_transfer::FileList> file_list,
                                 proto::file_transfer::Status status);

    virtual void OnDirectorySizeReply(const std::experimental::filesystem::path& path,
                                      uint64_t size,
                                      proto::file_transfer::Status status);

    virtual void OnCreateDirectoryReply(const std::experimental::filesystem::path& path,
                                        proto::file_transfer::Status status);

    virtual void OnRemoveReply(const std::experimental::filesystem::path& path,
                               proto::file_transfer::Status status);

    virtual void OnRenameReply(const std::experimental::filesystem::path& old_name,
                               const std::experimental::filesystem::path& new_name,
                               proto::file_transfer::Status status);

    virtual void OnFileUploadReply(const std::experimental::filesystem::path& file_path,
                                   proto::file_transfer::Status status);

    virtual void OnFileDownloadReply(const std::experimental::filesystem::path& file_path,
                                     proto::file_transfer::Status status);

    virtual void OnFilePacketSended(uint32_t flags, proto::file_transfer::Status status);

    virtual void OnFilePacketReceived(std::shared_ptr<proto::file_transfer::FilePacket> file_packet,
                                      proto::file_transfer::Status status);

private:
    friend class FileReplyReceiverProxy;

    std::shared_ptr<FileReplyReceiverProxy> receiver_proxy_;

    DISALLOW_COPY_AND_ASSIGN(FileReplyReceiver);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
