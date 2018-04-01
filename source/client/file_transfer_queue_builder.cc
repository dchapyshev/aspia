//
// PROJECT:         Aspia
// FILE:            client/file_transfer_queue_builder.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_queue_builder.h"

#include <QCoreApplication>

#include "client/file_request.h"
#include "client/file_status.h"

namespace aspia {

namespace {

const char* kReplySlot = "reply";

QString normalizePath(const QString& path)
{
    QString normalized_path = path;

    normalized_path.replace('\\', '/');
    if (!normalized_path.endsWith('/'))
        normalized_path += '/';

    return normalized_path;
}

} // namespace

FileTransferQueueBuilder::FileTransferQueueBuilder(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

FileTransferQueueBuilder::~FileTransferQueueBuilder() = default;

QQueue<FileTransferTask> FileTransferQueueBuilder::taskQueue() const
{
    return tasks_;
}

void FileTransferQueueBuilder::start(const QString& source_path,
                                     const QString& target_path,
                                     const QList<FileTransfer::Item>& items)
{
    emit started();

    for (const auto& item : items)
        addPendingTask(source_path, target_path, item.name, item.is_directory, item.size);

    processNextPendingTask();
}

void FileTransferQueueBuilder::reply(const proto::file_transfer::Request& request,
                                     const proto::file_transfer::Reply& reply)
{
    Q_ASSERT(!tasks_.isEmpty());

    if (!request.has_file_list_request())
    {
        processError(tr("An unexpected answer was received."));
        return;
    }

    if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
    {
        processError(tr("An error occurred while retrieving the list of files: %1")
                     .arg(fileStatusToString(reply.status())));
        return;
    }

    // If we get a list of files, then the last task is a directory.
    const FileTransferTask& last_task = tasks_.back();
    Q_ASSERT(last_task.isDirectory());

    for (int i = 0; i < reply.file_list().item_size(); ++i)
    {
        const proto::file_transfer::FileList::Item& item = reply.file_list().item(i);

        addPendingTask(last_task.sourcePath(),
                       last_task.targetPath(),
                       QString::fromStdString(item.name()),
                       item.is_directory(),
                       item.size());
    }

    processNextPendingTask();
}

void FileTransferQueueBuilder::processNextPendingTask()
{
    if (pending_tasks_.isEmpty())
    {
        emit finished();
        return;
    }

    tasks_.push_back(pending_tasks_.front());
    pending_tasks_.pop_front();

    const FileTransferTask& current = tasks_.back();
    if (!current.isDirectory())
    {
        processNextPendingTask();
        return;
    }

    emit request(FileRequest::fileListRequest(this, current.sourcePath(), kReplySlot));
}

void FileTransferQueueBuilder::processError(const QString& message)
{
    tasks_.clear();

    emit error(message);
    emit finished();
}

void FileTransferQueueBuilder::addPendingTask(const QString& source_dir,
                                              const QString& target_dir,
                                              const QString& item_name,
                                              bool is_directory,
                                              qint64 size)
{
    QString source_path = normalizePath(source_dir) + item_name;
    QString target_path = normalizePath(target_dir) + item_name;

    if (is_directory)
    {
        source_path = normalizePath(source_path);
        target_path = normalizePath(target_path);
    }

    pending_tasks_.push_back(FileTransferTask(source_path, target_path, is_directory, size));
}

} // namespace aspia
