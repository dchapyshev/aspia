//
// PROJECT:         Aspia
// FILE:            client/file_transfer_queue_builder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H

#include <QObject>
#include <QQueue>
#include <QPair>

#include "client/file_transfer_task.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

// The class prepares the task queue to perform the downloading/uploading.
class FileTransferQueueBuilder : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferQueueBuilder(QObject* parent = nullptr);
    ~FileTransferQueueBuilder();

    // Returns the queue of tasks.
    QQueue<FileTransferTask> taskQueue() const;

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
    void start(const QString& source_path,
               const QString& target_path,
               const QList<QPair<QString, bool>>& items);

    // Reads the reply to the request.
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);

private:
    void addPendingTask(const QString& source_dir,
                        const QString& target_dir,
                        const QString& item_name,
                        bool is_directory);
    void processNextPendingTask();
    void processError(const QString& message);

    QQueue<FileTransferTask> pending_tasks_;
    QQueue<FileTransferTask> tasks_;

    Q_DISABLE_COPY(FileTransferQueueBuilder)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H
