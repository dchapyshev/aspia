//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_uploader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_uploader.h"
#include "base/logging.h"

namespace aspia {

FileTransferUploader::FileTransferUploader(std::shared_ptr<FileRequestSenderProxy> sender,
                                           Delegate* delegate)
    : FileTransfer(std::move(sender), delegate)
{
    // Nothing
}

uint64_t FileTransferUploader::BuildTaskListForDirectoryContent(const FilePath& source_path,
                                                                const FilePath& target_path)
{
    std::error_code code;
    uint64_t size = 0;

    for (const auto& entry : std::experimental::filesystem::directory_iterator(source_path, code))
    {
        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(entry.path().filename());

        if (std::experimental::filesystem::is_directory(entry.status()))
        {
            task_queue_.push(Task(entry.path(), target_object_path, 0, true));
            size += BuildTaskListForDirectoryContent(entry.path(), target_object_path);
        }
        else
        {
            uintmax_t file_size = std::experimental::filesystem::file_size(entry.path(), code);

            if (file_size == static_cast<uintmax_t>(-1))
                file_size = 0;

            task_queue_.push(Task(entry.path(), target_object_path, file_size, false));

            size += file_size;
        }
    }

    return size;
}

void FileTransferUploader::Start(const FilePath& source_path,
                                 const FilePath& target_path,
                                 const FileList& file_list)
{
    uint64_t total_size = 0;
    std::error_code code;

    for (const auto& file : file_list)
    {
        FilePath source_object_path;

        source_object_path.assign(source_path);
        source_object_path.append(std::experimental::filesystem::u8path(file.name()));

        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(std::experimental::filesystem::u8path(file.name()));

        if (file.is_directory())
        {
            task_queue_.push(Task(source_object_path, target_object_path, 0, true));

            total_size += BuildTaskListForDirectoryContent(source_object_path,
                                                           target_object_path);
        }
        else
        {
            uintmax_t file_size =
                std::experimental::filesystem::file_size(source_object_path, code);

            if (file_size == static_cast<uintmax_t>(-1))
                file_size = 0;

            task_queue_.push(Task(source_object_path, target_object_path, file_size, false));

            total_size += file_size;
        }
    }

    delegate_->OnTransferStarted(source_path, target_path, total_size);

    // Run first task.
    RunTask(task_queue_.front(), FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::RunTask(const Task& task, FileRequestSender::Overwrite overwrite)
{
    if (task.IsDirectory())
    {
        sender_->SendCreateDirectoryRequest(This(), task.TargetPath());
    }
    else
    {
        sender_->SendFileUploadRequest(This(), task.TargetPath(), overwrite);
    }
}

void FileTransferUploader::RunNextTask()
{
    // Delete the task only after confirmation of its successful execution.
    if (!task_queue_.empty())
    {
        const Task& task = task_queue_.front();
        delegate_->OnObjectTransfer(task.SourcePath().filename(), task.Size(), 0);
        task_queue_.pop();
    }

    if (task_queue_.empty())
    {
        delegate_->OnTransferComplete();
        return;
    }

    const Task& task = task_queue_.front();

    delegate_->OnObjectTransfer(task.SourcePath().filename(), task.Size(), task.Size());

    // Run next task.
    RunTask(task, FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list)
{
    DLOG(FATAL) << "Unexpectedly received: drive list";
}

void FileTransferUploader::OnDriveListRequestFailure(proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: drive list";
}

void FileTransferUploader::OnFileListRequestReply(const FilePath& path,
                                                  std::unique_ptr<proto::FileList> file_list)
{
    DLOG(FATAL) << "Unexpectedly received: file list";
}

void FileTransferUploader::OnFileListRequestFailure(const FilePath& path,
                                                    proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: file list";
}

void FileTransferUploader::OnDirectorySizeRequestReply(const FilePath& path, uint64_t size)
{
    DLOG(FATAL) << "Unexpectedly received: directory size";
}

void FileTransferUploader::OnDirectorySizeRequestFailure(const FilePath& path,
                                                         proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: directory size";
}

void FileTransferUploader::OnUnableToCreateDirectoryAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            delegate_->OnTransferComplete();
            break;

        case Action::SKIP:
            RunNextTask();
            break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnCreateDirectoryRequestReply(const FilePath& path,
                                                         proto::RequestStatus status)
{
    if (status == proto::REQUEST_STATUS_SUCCESS ||
        status == proto::REQUEST_STATUS_PATH_ALREADY_EXISTS)
    {
        RunNextTask();
        return;
    }

    if (create_directory_failure_action_ != Action::ASK)
    {
        OnUnableToCreateDirectoryAction(create_directory_failure_action_);
        return;
    }

    ActionCallback callback = std::bind(
        &FileTransferUploader::OnUnableToCreateDirectoryAction, this, std::placeholders::_1);

    delegate_->OnUnableToCreateDirectory(path, status, std::move(callback));
}

void FileTransferUploader::OnRemoveRequestReply(const FilePath& path, proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly reply: remove";
}

void FileTransferUploader::OnRenameRequestReply(const FilePath& old_name,
                                                const FilePath& new_name,
                                                proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly reply: rename";
}

void FileTransferUploader::OnUnableToCreateFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            delegate_->OnTransferComplete();
            break;

        case Action::REPLACE:
        case Action::REPLACE_ALL:
        {
            if (action == Action::REPLACE_ALL)
                file_create_failure_action_ = action;

            RunTask(task_queue_.front(), FileRequestSender::Overwrite::YES);
        }
        break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_create_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnUnableToOpenFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            delegate_->OnTransferComplete();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_open_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnUnableToReadFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_open_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnFileUploadRequestReply(const FilePath& file_path,
                                                    proto::RequestStatus status)
{
    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        if (file_create_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToCreateFileAction, this, std::placeholders::_1);

            delegate_->OnUnableToCreateFile(file_path, status, std::move(callback));
        }
        else
        {
            OnUnableToCreateFileAction(file_create_failure_action_);
        }

        return;
    }

    const Task& current_task = task_queue_.front();

    file_packetizer_ = FilePacketizer::Create(current_task.SourcePath());
    if (!file_packetizer_)
    {
        if (file_open_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToOpenFileAction, this, std::placeholders::_1);

            delegate_->OnUnableToOpenFile(current_task.SourcePath(),
                                          proto::REQUEST_STATUS_ACCESS_DENIED,
                                          std::move(callback));
        }
        else
        {
            OnUnableToOpenFileAction(file_open_failure_action_);
        }

        return;
    }

    std::unique_ptr<proto::FilePacket> file_packet = file_packetizer_->CreateNextPacket();
    if (!file_packet)
    {
        if (file_read_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToReadFileAction, this, std::placeholders::_1);

            delegate_->OnUnableToReadFile(current_task.SourcePath(),
                                          proto::REQUEST_STATUS_ACCESS_DENIED,
                                          std::move(callback));
        }
        else
        {
            OnUnableToReadFileAction(file_read_failure_action_);
        }

        return;
    }

    sender_->SendFileUploadDataRequest(This(), std::move(file_packet));
}

void FileTransferUploader::OnUnableToWriteFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            delegate_->OnTransferComplete();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_write_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnFileUploadDataRequestReply(
    std::unique_ptr<proto::FilePacket> prev_file_packet,
    proto::RequestStatus status)
{
    if (!file_packetizer_)
    {
        LOG(ERROR) << "Unexpected reply: file upload data";
        delegate_->OnTransferComplete();
        return;
    }

    if (status != proto::RequestStatus::REQUEST_STATUS_SUCCESS)
    {
        if (file_write_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToWriteFileAction, this, std::placeholders::_1);

            const Task& current_task = task_queue_.front();

            delegate_->OnUnableToWriteFile(current_task.TargetPath(), status, std::move(callback));
        }
        else
        {
            OnUnableToWriteFileAction(file_write_failure_action_);
        }

        return;
    }

    if (prev_file_packet->flags() & proto::FilePacket::LAST_PACKET)
    {
        file_packetizer_.reset();
        RunNextTask();
        return;
    }

    const Task& current_task = task_queue_.front();

    delegate_->OnObjectTransfer(current_task.SourcePath().filename(),
                                current_task.Size(),
                                file_packetizer_->LeftSize());

    std::unique_ptr<proto::FilePacket> new_file_packet = file_packetizer_->CreateNextPacket();
    if (!new_file_packet)
    {
        if (file_read_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToReadFileAction, this, std::placeholders::_1);

            delegate_->OnUnableToReadFile(current_task.SourcePath(),
                                          proto::REQUEST_STATUS_ACCESS_DENIED,
                                          std::move(callback));
        }
        else
        {
            OnUnableToReadFileAction(file_read_failure_action_);
        }

        return;
    }

    sender_->SendFileUploadDataRequest(This(), std::move(new_file_packet));
}

} // namespace aspia
