//
// PROJECT:         Aspia
// FILE:            client/file_transfer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer.h"

#include "client/file_status.h"
#include "client/file_transfer_queue_builder.h"

namespace aspia {

namespace {

const char* kSourceReplySlot = "sourceReply";
const char* kTargetReplySlot = "targetReply";

} // namespace

FileTransfer::FileTransfer(Type type, QObject* parent)
    : QObject(parent),
      type_(type)
{
    actions_.insert(OtherError, QPair<Actions, Action>(Abort, Ask));
    actions_.insert(DirectoryCreateError,
                    QPair<Actions, Action>(Abort | Skip | SkipAll, Ask));
    actions_.insert(FileCreateError,
                    QPair<Actions, Action>(Abort | Skip | SkipAll, Ask));
    actions_.insert(FileOpenError,
                    QPair<Actions, Action>(Abort | Skip | SkipAll, Ask));
    actions_.insert(FileAlreadyExists,
                    QPair<Actions, Action>(Abort | Skip | SkipAll | Replace | ReplaceAll, Ask));
    actions_.insert(FileWriteError,
                    QPair<Actions, Action>(Abort | Skip | SkipAll, Ask));
    actions_.insert(FileReadError,
                    QPair<Actions, Action>(Abort | Skip | SkipAll, Ask));
}

void FileTransfer::start(const QString& source_path,
                         const QString& target_path,
                         const QList<Item>& items)
{
    builder_ = new FileTransferQueueBuilder();

    connect(builder_, &FileTransferQueueBuilder::started, this, &FileTransfer::started);
    connect(builder_, &FileTransferQueueBuilder::error, this, &FileTransfer::taskQueueError);
    connect(builder_, &FileTransferQueueBuilder::finished, this, &FileTransfer::taskQueueReady);

    if (type_ == Downloader)
    {
        connect(builder_, &FileTransferQueueBuilder::request, this, &FileTransfer::remoteRequest);
    }
    else
    {
        Q_ASSERT(type_ == Uploader);
        connect(builder_, &FileTransferQueueBuilder::request, this, &FileTransfer::localRequest);
    }

    connect(builder_, &FileTransferQueueBuilder::finished,
            builder_, &FileTransferQueueBuilder::deleteLater);

    builder_->start(source_path, target_path, items);
}

FileTransfer::Actions FileTransfer::availableActions(Error error_type) const
{
    return actions_[error_type].first;
}

FileTransfer::Action FileTransfer::defaultAction(Error error_type) const
{
    return actions_[error_type].second;
}

void FileTransfer::setDefaultAction(Error error_type, Action action)
{
    actions_[error_type].second = action;
}

FileTransferTask& FileTransfer::currentTask()
{
    return tasks_.front();
}

void FileTransfer::targetReply(const proto::file_transfer::Request& request,
                               const proto::file_transfer::Reply& reply)
{
    if (request.has_create_directory_request())
    {
        if (reply.status() == proto::file_transfer::STATUS_SUCCESS ||
            reply.status() == proto::file_transfer::STATUS_PATH_ALREADY_EXISTS)
        {
            processNextTask();
            return;
        }

        processError(DirectoryCreateError,
                     tr("Failed to create directory \"%1\": %2")
                     .arg(currentTask().targetPath())
                     .arg(fileStatusToString(reply.status())));
    }
    else if (request.has_upload_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            Error error_type = FileCreateError;

            if (reply.status() == proto::file_transfer::STATUS_PATH_ALREADY_EXISTS)
                error_type = FileAlreadyExists;

            processError(error_type,
                         tr("Failed to create file \"%1\": %2")
                         .arg(currentTask().targetPath())
                         .arg(fileStatusToString(reply.status())));
            return;
        }

        sourceRequest(FileRequest::packetRequest(this, kSourceReplySlot));
    }
    else if (request.has_packet())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            processError(FileWriteError,
                         tr("Failed to write file: \"%1\": %2")
                         .arg(currentTask().targetPath())
                         .arg(fileStatusToString(reply.status())));
            return;
        }

        if (currentTask().size() && total_size_)
        {
            qint64 packet_size = request.packet().data().size();

            task_transfered_size_ += packet_size;
            total_transfered_size_ += packet_size;

            int task_percentage = task_transfered_size_ * 100 / currentTask().size();
            int total_percentage = total_transfered_size_ * 100 / total_size_;

            if (task_percentage != task_percentage_ || total_percentage != total_percentage_)
            {
                task_percentage_ = task_percentage;
                total_percentage_ = total_percentage;

                emit progressChanged(total_percentage_, task_percentage_);
            }
        }

        if (request.packet().flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
        {
            processNextTask();
            return;
        }

        sourceRequest(FileRequest::packetRequest(this, kSourceReplySlot));
    }
    else
    {
        emit error(this, OtherError, tr("An unexpected response to the request was received"));
    }
}

void FileTransfer::sourceReply(const proto::file_transfer::Request& request,
                               const proto::file_transfer::Reply& reply)
{
    if (request.has_download_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            processError(FileOpenError,
                         tr("Failed to open file: \"%1\": %2")
                         .arg(currentTask().sourcePath()
                         .arg(fileStatusToString(reply.status()))));
            return;
        }

        targetRequest(FileRequest::uploadRequest(
            this,
            currentTask().targetPath(),
            currentTask().overwrite(),
            kTargetReplySlot));
    }
    else if (request.has_packet_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            processError(FileReadError,
                         tr("Failed to read file \"%1\": %2")
                         .arg(currentTask().sourcePath())
                         .arg(fileStatusToString(reply.status())));
            return;
        }

        targetRequest(FileRequest::packet(this, reply.packet(), kTargetReplySlot));
    }
    else
    {
        emit error(this, OtherError, tr("An unexpected response to the request was received"));
    }
}

void FileTransfer::taskQueueError(const QString& message)
{
    emit error(this, OtherError, message);
}

void FileTransfer::taskQueueReady()
{
    Q_ASSERT(builder_ != nullptr);

    tasks_ = builder_->taskQueue();

    for (const auto& task : tasks_)
        total_size_ += task.size();

    processTask(false);
}

void FileTransfer::applyAction(Error error_type, Action action)
{
    switch (action)
    {
        case Action::Abort:
            emit finished();
            break;

        case Action::Replace:
        case Action::ReplaceAll:
        {
            if (action == Action::ReplaceAll)
                setDefaultAction(error_type, action);

            processTask(true);
        }
        break;

        case Action::Skip:
        case Action::SkipAll:
        {
            if (action == Action::SkipAll)
                setDefaultAction(error_type, action);

            processNextTask();
        }
        break;

        default:
            qFatal("Unexpected action");
            break;
    }
}

void FileTransfer::processTask(bool overwrite)
{
    task_percentage_ = 0;
    task_transfered_size_ = 0;

    FileTransferTask& task = currentTask();

    task.setOverwrite(overwrite);

    emit currentItemChanged(task.sourcePath(), task.targetPath());

    if (task.isDirectory())
        targetRequest(FileRequest::createDirectoryRequest(this, task.targetPath(), kTargetReplySlot));
    else
        sourceRequest(FileRequest::downloadRequest(this, task.sourcePath(), kSourceReplySlot));
}

void FileTransfer::processNextTask()
{
    if (!tasks_.isEmpty())
    {
        // Delete the task only after confirmation of its successful execution.
        tasks_.pop_front();
    }

    if (tasks_.isEmpty())
    {
        emit finished();
        return;
    }

    processTask(false);
}

void FileTransfer::processError(Error error_type, const QString& message)
{
    Action action = defaultAction(error_type);
    if (action != Ask)
    {
        applyAction(error_type, action);
        return;
    }

    emit error(this, error_type, message);
}

void FileTransfer::sourceRequest(FileRequest* request)
{
    if (type_ == Downloader)
    {
        QMetaObject::invokeMethod(this, "remoteRequest", Q_ARG(FileRequest*, request));
    }
    else
    {
        Q_ASSERT(type_ == Uploader);
        QMetaObject::invokeMethod(this, "localRequest", Q_ARG(FileRequest*, request));
    }
}

void FileTransfer::targetRequest(FileRequest* request)
{
    if (type_ == Downloader)
    {
        QMetaObject::invokeMethod(this, "localRequest", Q_ARG(FileRequest*, request));
    }
    else
    {
        Q_ASSERT(type_ == Uploader);
        QMetaObject::invokeMethod(this, "remoteRequest", Q_ARG(FileRequest*, request));
    }
}

} // namespace aspia
