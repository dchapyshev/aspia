//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_task_queue_builder.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_task_queue_builder.h"
#include "base/logging.h"

namespace aspia {

FileTaskQueueBuilder::FileTaskQueueBuilder()
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

FileTaskQueueBuilder::~FileTaskQueueBuilder()
{
    thread_.Stop();
}

void FileTaskQueueBuilder::Start(std::shared_ptr<FileRequestSenderProxy> sender,
                                 const FilePath& source_path,
                                 const FilePath& target_path,
                                 const FileList& file_list,
                                 FinishCallback callback)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileTaskQueueBuilder::Start, this, sender,
                                    source_path, target_path, file_list, callback));
        return;
    }

    finish_callback_ = std::move(callback);
    sender_ = sender;

    DCHECK(finish_callback_ != nullptr);
    DCHECK(!file_list.empty());

    for (const auto& file : file_list)
    {
        AddIncomingTask(source_path, target_path, file);
    }

    ProcessNextIncommingTask();
}

void FileTaskQueueBuilder::OnFileListReply(const FilePath& path,
                                           std::shared_ptr<proto::FileList> file_list,
                                           proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTaskQueueBuilder::OnFileListReply, this, path, file_list, status));
        return;
    }

    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        runner_->PostQuit();
        return;
    }

    DCHECK(!pending_task_queue_.empty());
    const FileTask& last_task = pending_task_queue_.back();
    DCHECK(last_task.IsDirectory());

    for (int index = 0; index < file_list->item_size(); ++index)
    {
        AddIncomingTask(last_task.SourcePath(), last_task.TargetPath(), file_list->item(index));
    }

    ProcessNextIncommingTask();
}

void FileTaskQueueBuilder::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void FileTaskQueueBuilder::OnAfterThreadRunning()
{
    // Nothing
}

void FileTaskQueueBuilder::AddIncomingTask(const FilePath& source_path,
                                           const FilePath& target_path,
                                           const proto::FileList::Item& file)
{
    DCHECK(runner_->BelongsToCurrentThread());

    FilePath object_name(std::experimental::filesystem::u8path(file.name()));

    FilePath source_object_path(source_path);
    source_object_path.append(object_name);

    FilePath target_object_path(target_path);
    target_object_path.append(object_name);

    incoming_task_queue_.emplace_back(source_object_path, target_object_path,
                                      file.size(), file.is_directory());
}

void FileTaskQueueBuilder::FrontIncomingToBackPending()
{
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(!incoming_task_queue_.empty());

    FileTask current_task = std::move(incoming_task_queue_.front());
    incoming_task_queue_.pop_front();

    task_object_size_ += current_task.Size();
    ++task_object_count_;

    pending_task_queue_.emplace_back(std::move(current_task));
}

void FileTaskQueueBuilder::ProcessNextIncommingTask()
{
    DCHECK(runner_->BelongsToCurrentThread());

    if (incoming_task_queue_.empty())
    {
        if (finish_callback_ != nullptr)
            finish_callback_(pending_task_queue_, task_object_size_, task_object_count_);
        return;
    }

    FrontIncomingToBackPending();

    const FileTask& current_task = pending_task_queue_.back();

    if (current_task.IsDirectory())
    {
        sender_->SendFileListRequest(This(), current_task.SourcePath());
    }
    else
    {
        ProcessNextIncommingTask();
    }
}

} // namespace aspia
