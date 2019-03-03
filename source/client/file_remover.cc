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

#include "client/file_remover.h"
#include "base/logging.h"
#include "client/file_remove_queue_builder.h"
#include "client/file_status.h"

namespace client {

FileRemover::FileRemover(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

void FileRemover::start(const QString& path, const QList<Item>& items)
{
    builder_ = new FileRemoveQueueBuilder();

    connect(builder_, &FileRemoveQueueBuilder::started,
            this, &FileRemover::started);

    connect(builder_, &FileRemoveQueueBuilder::error,
            this, &FileRemover::taskQueueError);

    connect(builder_, &FileRemoveQueueBuilder::finished,
            this, &FileRemover::taskQueueReady);

    connect(builder_, &FileRemoveQueueBuilder::finished,
            builder_, &FileRemoveQueueBuilder::deleteLater);

    connect(builder_, &FileRemoveQueueBuilder::newRequest,
            this, &FileRemover::newRequest);

    builder_->start(path, items);
}

void FileRemover::applyAction(Action action)
{
    switch (action)
    {
        case Skip:
            processNextTask();
            break;

        case SkipAll:
            failure_action_ = action;
            processNextTask();
            break;

        case Abort:
            emit finished();
            break;

        default:
            LOG(LS_FATAL) << "Unexpected action: " << action;
            break;
    }
}

void FileRemover::reply(const proto::file_transfer::Request& request,
                        const proto::file_transfer::Reply& reply)
{
    if (!request.has_remove_request())
    {
        emit error(this, Abort, tr("An unexpected answer was received."));
        return;
    }

    if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
    {
        Actions actions;

        switch (reply.status())
        {
            case proto::file_transfer::STATUS_PATH_NOT_FOUND:
            case proto::file_transfer::STATUS_ACCESS_DENIED:
            {
                if (failure_action_ != Ask)
                {
                    applyAction(failure_action_);
                    return;
                }

                actions = Abort | Skip | SkipAll;
            }
            break;

            default:
                actions = Abort;
                break;
        }

        emit error(this, actions, tr("Failed to delete \"%1\": %2.")
                   .arg(QString::fromStdString(request.remove_request().path()))
                   .arg(fileStatusToString(reply.status())));
        return;
    }

    processNextTask();
}

void FileRemover::taskQueueError(const QString& message)
{
    emit error(this, Abort, message);
}

void FileRemover::taskQueueReady()
{
    DCHECK(builder_ != nullptr);

    tasks_ = builder_->taskQueue();
    tasks_count_ = tasks_.size();

    processTask();
}

void FileRemover::processTask()
{
    if (tasks_.isEmpty())
    {
        emit finished();
        return;
    }

    DCHECK(tasks_count_ != 0);

    int percentage = (tasks_count_ - tasks_.size()) * 100 / tasks_count_;

    emit progressChanged(tasks_.front().path(), percentage);

    sendRequest(common::FileRequest::removeRequest(tasks_.front().path()));
}

void FileRemover::processNextTask()
{
    if (!tasks_.isEmpty())
        tasks_.pop_front();

    processTask();
}

void FileRemover::sendRequest(common::FileRequest* request)
{
    connect(request, &common::FileRequest::replyReady, this, &FileRemover::reply);
    emit newRequest(request);
}

} // namespace client
