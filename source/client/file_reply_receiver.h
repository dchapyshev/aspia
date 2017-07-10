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

    virtual void OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list) = 0;

    virtual void OnDriveListRequestFailure(proto::RequestStatus status) = 0;

    virtual void OnFileListRequestReply(const FilePath& path,
                                        std::unique_ptr<proto::FileList> file_list) = 0;

    virtual void OnFileListRequestFailure(const FilePath& path,
                                          proto::RequestStatus status) = 0;

    virtual void OnDirectorySizeRequestReply(const FilePath& path,
                                             uint64_t size) = 0;

    virtual void OnDirectorySizeRequestFailure(const FilePath& path,
                                               proto::RequestStatus status) = 0;

    virtual void OnCreateDirectoryRequestReply(const FilePath& path,
                                               proto::RequestStatus status) = 0;

    virtual void OnRemoveRequestReply(const FilePath& path,
                                      proto::RequestStatus status) = 0;

    virtual void OnRenameRequestReply(const FilePath& old_name,
                                      const FilePath& new_name,
                                      proto::RequestStatus status) = 0;

private:
    friend class FileReplyReceiverProxy;

    std::shared_ptr<FileReplyReceiverProxy> receiver_proxy_;

    DISALLOW_COPY_AND_ASSIGN(FileReplyReceiver);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
