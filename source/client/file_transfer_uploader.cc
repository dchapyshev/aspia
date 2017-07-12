//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_uploader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_uploader.h"
#include "base/logging.h"

namespace aspia {

namespace fs = std::experimental::filesystem;

FileTransferUploader::FileTransferUploader(
    std::shared_ptr<FileRequestSenderProxy> sender,
    Delegate* delegate)
    : FileTransfer(std::move(sender), delegate)
{
    // Nothing
}

uint64_t FileTransferUploader::BuildTaskListForDirectoryContent(
    const FilePath& source_path,
    const FilePath& target_path)
{
    std::error_code code;
    uint64_t size = 0;

    for (const auto& entry : fs::directory_iterator(source_path, code))
    {
        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(entry.path().filename());

        if (fs::is_directory(entry.status()))
        {
            task_queue_.push(Task(entry.path(), target_object_path, true));

            size += BuildTaskListForDirectoryContent(entry.path(),
                                                     target_object_path);
        }
        else
        {
            task_queue_.push(Task(entry.path(), target_object_path, false));

            size += fs::file_size(entry.path(), code);
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
        source_object_path.append(fs::u8path(file.name()));

        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(fs::u8path(file.name()));

        if (file.is_directory())
        {
            task_queue_.push(Task(source_object_path, target_object_path, true));

            total_size += BuildTaskListForDirectoryContent(source_object_path,
                                                           target_object_path);
        }
        else
        {
            task_queue_.push(Task(source_object_path, target_object_path, false));

            uintmax_t file_size = fs::file_size(source_object_path, code);

            if (file_size != static_cast<uintmax_t>(-1))
                total_size += file_size;
        }
    }

    delegate_->OnObjectSizeNotify(total_size);

    // Run first task.
    RunTask(task_queue_.front());
}

void FileTransferUploader::RunTask(const Task& task)
{
    if (task.IsDirectory())
    {
        sender_->SendCreateDirectoryRequest(This(), task.TargetPath());
    }
    else
    {
        // TODO: Start file uploading.
    }
}

void FileTransferUploader::RunNextTask()
{
    // Delete the task only after confirmation of its successful execution.
    if (!task_queue_.empty())
        task_queue_.pop();

    if (task_queue_.empty())
    {
        delegate_->OnTransferCompletionNotify();
        return;
    }

    const Task& task = task_queue_.front();

    delegate_->OnObjectTransferNotify(task.SourcePath(), task.TargetPath());

    // Run next task.
    RunTask(task);
}

void FileTransferUploader::OnDriveListRequestReply(
    std::unique_ptr<proto::DriveList> drive_list)
{
    DLOG(FATAL) << "Unexpectedly received a list of drives";
}

void FileTransferUploader::OnDriveListRequestFailure(
    proto::RequestStatus status)
{
    // TODO
}

void FileTransferUploader::OnFileListRequestReply(
    const FilePath& path,
    std::unique_ptr<proto::FileList> file_list)
{
    DLOG(FATAL) << "Unexpectedly received a list of file";
}

void FileTransferUploader::OnFileListRequestFailure(
    const FilePath& path,
    proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received a list of files";
}

void FileTransferUploader::OnDirectorySizeRequestReply(
    const FilePath& path,
    uint64_t size)
{
    // TODO
}

void FileTransferUploader::OnDirectorySizeRequestFailure(
    const FilePath& path,
    proto::RequestStatus status)
{
    // TODO
}

void FileTransferUploader::OnCreateDirectoryRequestReply(
    const FilePath& path,
    proto::RequestStatus status)
{
    RunNextTask();
}

void FileTransferUploader::OnRemoveRequestReply(
    const FilePath& path,
    proto::RequestStatus status)
{
    // TODO
}

void FileTransferUploader::OnRenameRequestReply(
    const FilePath& old_name,
    const FilePath& new_name,
    proto::RequestStatus status)
{
    // TODO
}

} // namespace aspia
