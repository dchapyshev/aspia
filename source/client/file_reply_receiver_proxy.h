//
// PROJECT:         Aspia
// FILE:            client/file_reply_receiver_proxy.h
// LICENSE:         GNU Lesser General Public License 2.1
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
    bool OnDriveListReply(std::shared_ptr<proto::file_transfer::DriveList> drive_list,
                          proto::file_transfer::Status status);

    bool OnFileListReply(const std::experimental::filesystem::path& path,
                         std::shared_ptr<proto::file_transfer::FileList> file_list,
                         proto::file_transfer::Status status);

    bool OnDirectorySizeReply(const std::experimental::filesystem::path& path,
                              uint64_t size,
                              proto::file_transfer::Status status);

    bool OnCreateDirectoryReply(const std::experimental::filesystem::path& path,
                                proto::file_transfer::Status status);

    bool OnRemoveReply(const std::experimental::filesystem::path& path,
                       proto::file_transfer::Status status);

    bool OnRenameReply(const std::experimental::filesystem::path& old_name,
                       const std::experimental::filesystem::path& new_name,
                       proto::file_transfer::Status status);

    bool OnFileUploadReply(const std::experimental::filesystem::path& file_path,
                           proto::file_transfer::Status status);

    bool OnFileDownloadReply(const std::experimental::filesystem::path& file_path,
                             proto::file_transfer::Status status);

    bool OnFilePacketSended(uint32_t flags, proto::file_transfer::Status status);

    bool OnFilePacketReceived(std::shared_ptr<proto::file_transfer::FilePacket> file_packet,
                              proto::file_transfer::Status status);

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
