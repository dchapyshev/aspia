//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver_queue.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver_queue.h"
#include "base/logging.h"

namespace aspia {

bool FileReplyReceiverQueue::ProcessNextReply(
    std::unique_ptr<proto::file_transfer::HostToClient> reply)
{
    Request request;
    Receiver receiver;

    {
        std::lock_guard<std::mutex> lock(queue_lock_);

        if (queue_.empty())
        {
            LOG(ERROR) << "Unexpected message received. Receiver queue is empty";
            return false;
        }

        request = std::move(queue_.front().first);
        receiver = std::move(queue_.front().second);

        queue_.pop();
    }

    DCHECK(request);
    DCHECK(receiver);

    if (reply->type() != request->type())
    {
        LOG(ERROR) << "Request type does not match type of reply. Request type: "
            << request->type() << " Reply type: " << reply->type();
        return false;
    }

    switch (reply->type())
    {
        case proto::RequestType::REQUEST_TYPE_DRIVE_LIST:
        {
            if (reply->status() == proto::RequestStatus::REQUEST_STATUS_SUCCESS)
            {
                if (!reply->has_drive_list())
                    return false;

                std::unique_ptr<proto::DriveList> drive_list(reply->release_drive_list());
                receiver->OnDriveListRequestReply(std::move(drive_list));
            }
            else
            {
                receiver->OnDriveListRequestFailure(reply->status());
            }
        }
        break;

        case proto::RequestType::REQUEST_TYPE_FILE_LIST:
        {
            if (reply->status() == proto::RequestStatus::REQUEST_STATUS_SUCCESS)
            {
                if (!reply->has_file_list())
                    return false;

                std::unique_ptr<proto::FileList> file_list(reply->release_file_list());
                receiver->OnFileListRequestReply(std::move(file_list));
            }
            else
            {
                receiver->OnFileListRequestFailure(reply->status());
            }
        }
        break;

        case proto::RequestType::REQUEST_TYPE_DIRECTORY_SIZE:
        {
            if (reply->status() == proto::RequestStatus::REQUEST_STATUS_SUCCESS)
            {
                if (!reply->has_directory_size())
                    return false;

                receiver->OnDirectorySizeRequestReply(reply->directory_size().size());
            }
            else
            {
                receiver->OnDirectorySizeRequestFailure(reply->status());
            }
        }
        break;

        case proto::RequestType::REQUEST_TYPE_CREATE_DIRECTORY:
        {
            receiver->OnCreateDirectoryRequestReply(reply->status());
        }
        break;

        case proto::RequestType::REQUEST_TYPE_RENAME:
        {
            receiver->OnRenameRequestReply(reply->status());
        }
        break;

        case proto::RequestType::REQUEST_TYPE_REMOVE:
        {
            receiver->OnRemoveRequestReply(reply->status());
        }
        break;
    }

    return true;
}

void FileReplyReceiverQueue::Add(Request request, Receiver receiver)
{
    std::lock_guard<std::mutex> lock(queue_lock_);

    queue_.push(std::make_pair<Request, Receiver>(std::move(request),
                                                  std::move(receiver)));
}

} // namespace aspia
