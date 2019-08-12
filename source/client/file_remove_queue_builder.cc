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

#include "client/file_remove_queue_builder.h"

#include "client/file_status.h"
#include "common/file_request.h"

#include <QCoreApplication>

namespace client {

FileRemoveQueueBuilder::FileRemoveQueueBuilder(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

QQueue<FileRemoveTask> FileRemoveQueueBuilder::taskQueue() const
{
    return tasks_;
}

void FileRemoveQueueBuilder::start(const QString& path, const QList<FileRemover::Item>& items)
{
    emit started();

    for (const auto& item : items)
        pending_tasks_.push_back(FileRemoveTask(path + item.name, item.is_directory));

    processNextPendingTask();
}

void FileRemoveQueueBuilder::reply(const proto::FileRequest& request, const proto::FileReply& reply)
{
    if (!request.has_file_list_request())
    {
        processError(tr("An unexpected answer was received."));
        return;
    }

    if (reply.status() != proto::FileReply::STATUS_SUCCESS)
    {
        processError(tr("An error occurred while retrieving the list of files: %1")
                     .arg(fileStatusToString(reply.status())));
        return;
    }

    QString path = QString::fromStdString(request.file_list_request().path());
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');

    for (int i = 0; i < reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = reply.file_list().item(i);

        pending_tasks_.push_back(
            FileRemoveTask(path + QString::fromStdString(item.name()),
                           item.is_directory()));
    }

    processNextPendingTask();
}

void FileRemoveQueueBuilder::processNextPendingTask()
{
    if (pending_tasks_.isEmpty())
    {
        emit finished();
        return;
    }

    tasks_.push_front(pending_tasks_.front());
    pending_tasks_.pop_front();

    const FileRemoveTask& current = tasks_.front();
    if (!current.isDirectory())
    {
        processNextPendingTask();
        return;
    }

    sendRequest(common::FileRequest::fileListRequest(current.path()));
}

void FileRemoveQueueBuilder::processError(const QString& message)
{
    tasks_.clear();

    emit error(message);
    emit finished();
}

void FileRemoveQueueBuilder::sendRequest(common::FileRequest* request)
{
    connect(request, &common::FileRequest::replyReady, this, &FileRemoveQueueBuilder::reply);
    emit newRequest(request);
}

} // namespace client
