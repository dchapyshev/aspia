//
// PROJECT:         Aspia
// FILE:            client/file_remove_queue_builder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVE_QUEUE_BUILDER_H
#define _ASPIA_CLIENT__FILE_REMOVE_QUEUE_BUILDER_H

#include <QObject>
#include <QQueue>

#include "client/file_remove_task.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

// The class prepares the task queue to perform the deletion.
class FileRemoveQueueBuilder : public QObject
{
    Q_OBJECT

public:
    FileRemoveQueueBuilder(QObject* parent = nullptr);
    ~FileRemoveQueueBuilder();

    // Returns the queue of tasks.
    QQueue<FileRemoveTask> taskQueue() const;

signals:
    // Signals about the start of execution.
    void started();

    // Signals about the end of execution.
    void finished();

    // Signals an error when building a task queue. |message| contains a description of the error.
    void error(const QString& message);

    // Signals an outbound request.
    void request(const proto::file_transfer::Request& request,
                 const FileReplyReceiver& receiver);

public slots:
    // Starts building of the task queue.
    void start(const QList<FileRemoveTask>& tasks);

    // Reads the reply to the request.
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);

private:
    void processNextPendingTask();
    void processError(const QString& message);

    QQueue<FileRemoveTask> pending_tasks_;
    QQueue<FileRemoveTask> tasks_;

    Q_DISABLE_COPY(FileRemoveQueueBuilder)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVE_QUEUE_BUILDER_H
