//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/file_transfer_queue_builder.h"
#include "base/logging.h"
#include "client/file_status.h"
#include "common/file_request.h"

#include <QCoreApplication>

namespace client {

namespace {

QString normalizePath(const QString& path)
{
    QString normalized_path = path;

    normalized_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!normalized_path.endsWith(QLatin1Char('/')))
        normalized_path += QLatin1Char('/');

    return normalized_path;
}

} // namespace

FileTransferQueueBuilder::FileTransferQueueBuilder(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

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
    DCHECK(!tasks_.isEmpty());

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
    DCHECK(last_task.isDirectory());

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

    sendRequest(common::FileRequest::fileListRequest(current.sourcePath()));
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

void FileTransferQueueBuilder::sendRequest(common::FileRequest* request)
{
    connect(request, &common::FileRequest::replyReady, this, &FileTransferQueueBuilder::reply);
    emit newRequest(request);
}

} // namespace client
