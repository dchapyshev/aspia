//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_H

#include "base/files/file_path.h"
#include "client/file_reply_receiver.h"
#include "client/file_request_sender_proxy.h"

namespace aspia {

class FileTransfer
    : public FileReplyReceiver
{
public:
    enum class Action
    {
        ASK         = 0,
        ABORT       = 1,
        SKIP        = 2,
        SKIP_ALL    = 3,
        REPLACE     = 4,
        REPLACE_ALL = 5
    };

    using ActionCallback = std::function<void(Action action)>;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void OnTransferStarted(const FilePath& source_path,
                                       const FilePath& target_path,
                                       uint64_t size) = 0;

        virtual void OnTransferComplete() = 0;

        virtual void OnUnableToCreateDirectory(const FilePath& directory_path,
                                               proto::RequestStatus status,
                                               ActionCallback callback) = 0;

        virtual void OnUnableToCreateFile(const FilePath& file_path,
                                          proto::RequestStatus status,
                                          ActionCallback callback) = 0;

        virtual void OnUnableToOpenFile(const FilePath& file_path,
                                        proto::RequestStatus status,
                                        ActionCallback callback) = 0;

        virtual void OnUnableToWriteFile(const FilePath& file_path,
                                         proto::RequestStatus status,
                                         ActionCallback callback) = 0;

        virtual void OnUnableToReadFile(const FilePath& file_path,
                                        proto::RequestStatus status,
                                        ActionCallback callback) = 0;

        virtual void OnObjectTransfer(const FilePath& object_name,
                                      uint64_t total_object_size,
                                      uint64_t left_object_size) = 0;
    };

    FileTransfer(std::shared_ptr<FileRequestSenderProxy> sender, Delegate* delegate)
        : sender_(std::move(sender)),
          delegate_(delegate)
    {
        // Nothing
    }

    virtual ~FileTransfer() = default;

    using FileList = std::vector<proto::FileList::Item>;

    virtual void Start(const FilePath& source_path,
                       const FilePath& target_path,
                       const FileList& file_list) = 0;

protected:
    std::shared_ptr<FileRequestSenderProxy> sender_;
    Delegate* delegate_;

    Action create_directory_failure_action_ = Action::ASK;
    Action file_create_failure_action_ = Action::ASK;
    Action file_open_failure_action_ = Action::ASK;
    Action file_write_failure_action_ = Action::ASK;
    Action file_read_failure_action_ = Action::ASK;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_H
