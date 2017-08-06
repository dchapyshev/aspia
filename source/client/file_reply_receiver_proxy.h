//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H

#include "base/macros.h"
#include "client/file_reply_receiver.h"

#include <mutex>

namespace aspia {

class FileReplyReceiverProxy
{
public:
    bool OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list);

    bool OnDriveListRequestFailure(proto::RequestStatus status);

    bool OnFileListRequestReply(const FilePath& path, std::unique_ptr<proto::FileList> file_list);

    bool OnFileListRequestFailure(const FilePath& path, proto::RequestStatus status);

    bool OnDirectorySizeRequestReply(const FilePath& path, uint64_t size);

    bool OnDirectorySizeRequestFailure(const FilePath& path, proto::RequestStatus status);

    bool OnCreateDirectoryRequestReply(const FilePath& path, proto::RequestStatus status);

    bool OnRemoveRequestReply(const FilePath& path, proto::RequestStatus status);

    bool OnRenameRequestReply(const FilePath& old_name,
                              const FilePath& new_name,
                              proto::RequestStatus status);

    bool OnFileUploadRequestReply(const FilePath& file_path, proto::RequestStatus status);

    bool OnFileUploadDataRequestReply(std::unique_ptr<proto::FilePacket> file_packet,
                                      proto::RequestStatus status);

private:
    friend class FileReplyReceiver;

    explicit FileReplyReceiverProxy(FileReplyReceiver* receiver);

    // Called directly by FileReplyReceiver::~FileReplyReceiver.
    void WillDestroyCurrentReplyReceiver();

    FileReplyReceiver* receiver_;
    std::mutex receiver_lock_;

    DISALLOW_COPY_AND_ASSIGN(FileReplyReceiverProxy);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_PROXY_H
