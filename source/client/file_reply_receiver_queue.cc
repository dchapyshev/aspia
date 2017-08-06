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
    proto::file_transfer::HostToClient& reply)
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

    if (request->has_drive_list_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_drive_list())
                return false;

            std::unique_ptr<proto::DriveList> drive_list(reply.release_drive_list());

            receiver->OnDriveListRequestReply(std::move(drive_list));
        }
        else
        {
            receiver->OnDriveListRequestFailure(reply.status());
        }
    }
    else if (request->has_file_list_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_file_list())
                return false;

            std::unique_ptr<proto::FileList> file_list(reply.release_file_list());

            receiver->OnFileListRequestReply(
                std::experimental::filesystem::u8path(request->file_list_request().path()),
                std::move(file_list));
        }
        else
        {
            receiver->OnFileListRequestFailure(
                std::experimental::filesystem::u8path(
                    request->file_list_request().path()),
                reply.status());
        }
    }
    else if (request->has_directory_size_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_directory_size())
                return false;

            receiver->OnDirectorySizeRequestReply(
                std::experimental::filesystem::u8path(
                    request->directory_size_request().path()),
                reply.directory_size().size());
        }
        else
        {
            receiver->OnDirectorySizeRequestFailure(
                std::experimental::filesystem::u8path(
                    request->directory_size_request().path()),
                reply.status());
        }
    }
    else if (request->has_create_directory_request())
    {
        receiver->OnCreateDirectoryRequestReply(
            std::experimental::filesystem::u8path(
                request->create_directory_request().path()),
            reply.status());
    }
    else if (request->has_rename_request())
    {
        receiver->OnRenameRequestReply(
            std::experimental::filesystem::u8path(
                request->rename_request().old_name()),
            std::experimental::filesystem::u8path(
                request->rename_request().new_name()),
            reply.status());
    }
    else if (request->has_remove_request())
    {
        receiver->OnRemoveRequestReply(
            std::experimental::filesystem::u8path(
                request->remove_request().path()),
            reply.status());
    }
    else if (request->has_file_upload_request())
    {
        receiver->OnFileUploadRequestReply(
            std::experimental::filesystem::u8path(
                request->file_upload_request().file_path()),
            reply.status());
    }
    else if (request->has_file_packet())
    {
        std::unique_ptr<proto::FilePacket> file_packet(request->release_file_packet());

        receiver->OnFileUploadDataRequestReply(std::move(file_packet), reply.status());
    }
    else
    {
        DLOG(ERROR) << "Unhandled request type";
        return false;
    }

    return true;
}

void FileReplyReceiverQueue::Add(Request request, Receiver receiver)
{
    std::lock_guard<std::mutex> lock(queue_lock_);
    queue_.emplace(std::move(request), std::move(receiver));
}

} // namespace aspia
