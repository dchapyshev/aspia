//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H

#include "base/macros.h"
#include "base/files/file_path.h"
#include "proto/file_transfer_session.pb.h"

#include <memory>

namespace aspia {

class FileReplyReceiverProxy;

class FileReplyReceiver
{
public:
    FileReplyReceiver();
    virtual ~FileReplyReceiver();

protected:
    std::shared_ptr<FileReplyReceiverProxy> This();

    virtual void OnDriveListReply(std::shared_ptr<proto::DriveList> drive_list,
                                  proto::RequestStatus status) = 0;

    virtual void OnFileListReply(const FilePath& path,
                                 std::shared_ptr<proto::FileList> file_list,
                                 proto::RequestStatus status) = 0;

    virtual void OnDirectorySizeReply(const FilePath& path,
                                      uint64_t size,
                                      proto::RequestStatus status) = 0;

    virtual void OnCreateDirectoryReply(const FilePath& path,
                                        proto::RequestStatus status) = 0;

    virtual void OnRemoveReply(const FilePath& path, proto::RequestStatus status) = 0;

    virtual void OnRenameReply(const FilePath& old_name,
                               const FilePath& new_name,
                               proto::RequestStatus status) = 0;

    virtual void OnFileUploadReply(const FilePath& file_path,
                                   proto::RequestStatus status) = 0;

    virtual void OnFileDownloadReply(const FilePath& file_path,
                                     proto::RequestStatus status) = 0;

    virtual void OnFilePacketSended(uint32_t flags, proto::RequestStatus status) = 0;

    virtual void OnFilePacketReceived(std::shared_ptr<proto::FilePacket> file_packet,
                                      proto::RequestStatus status) = 0;

private:
    friend class FileReplyReceiverProxy;

    std::shared_ptr<FileReplyReceiverProxy> receiver_proxy_;

    DISALLOW_COPY_AND_ASSIGN(FileReplyReceiver);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
