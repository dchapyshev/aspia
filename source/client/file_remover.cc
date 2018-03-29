//
// PROJECT:         Aspia
// FILE:            client/file_remover.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_remover.h"

#include "client/file_remove_queue_builder.h"
#include "client/file_request.h"
#include "client/file_status.h"

namespace aspia {

FileRemover::FileRemover(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

FileRemover::~FileRemover() = default;

void FileRemover::start(const QList<FileRemoveTask>& tasks)
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

    connect(builder_, &FileRemoveQueueBuilder::request,
            this, &FileRemover::request);

    builder_->start(tasks);
}

void FileRemover::applyAction(Action action)
{
    switch (action)
    {
        case Action::Skip:
            processNextTask();
            break;

        case Action::SkipAll:
            failure_action_ = action;
            processNextTask();
            break;

        case Action::Abort:
            emit finished();
            break;

        default:
            qFatal("Unexpected action: %d", action);
            break;
    }
}

void FileRemover::reply(const proto::file_transfer::Request& request,
                        const proto::file_transfer::Reply& reply)
{
    if (!request.has_remove_request())
    {
        emit error(this, Action::Abort, tr("An unexpected answer was received."));
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
                if (failure_action_ != Action::Ask)
                {
                    applyAction(failure_action_);
                    return;
                }

                actions = Action::Abort | Action::Skip | Action::SkipAll;
            }
            break;

            default:
                actions = Action::Abort;
                break;
        }

        emit error(this, actions, tr("Failed to delete \"%1\": %2.")
                   .arg(QString::fromUtf8(request.remove_request().path().c_str(),
                                          request.remove_request().path().size()))
                   .arg(fileStatusToString(reply.status())));
        return;
    }

    processNextTask();
}

void FileRemover::taskQueueError(const QString& message)
{
    emit error(this, Action::Abort, message);
}

void FileRemover::taskQueueReady()
{
    Q_ASSERT(builder_ != nullptr);

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

    Q_ASSERT(tasks_count_ != 0);

    int percentage = (tasks_count_ - tasks_.size()) * 100 / tasks_count_;

    emit progressChanged(tasks_.front().path(), percentage);
    emit request(FileRequest::removeRequest(tasks_.front().path()),
                 FileReplyReceiver(this, "reply"));
}

void FileRemover::processNextTask()
{
    if (!tasks_.isEmpty())
        tasks_.pop_front();

    processTask();
}

} // namespace aspia
