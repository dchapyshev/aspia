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

#ifndef ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H_
#define ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H_

#include "client/file_transfer.h"

namespace aspia {

// The class prepares the task queue to perform the downloading/uploading.
class FileTransferQueueBuilder : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferQueueBuilder(QObject* parent = nullptr);
    ~FileTransferQueueBuilder() = default;

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
    void newRequest(FileRequest* request);

public slots:
    // Starts building of the task queue.
    void start(const QString& source_path,
               const QString& target_path,
               const QList<FileTransfer::Item>& items);

    // Reads the reply to the request.
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);

private:
    void addPendingTask(const QString& source_dir,
                        const QString& target_dir,
                        const QString& item_name,
                        bool is_directory,
                        qint64 size);
    void processNextPendingTask();
    void processError(const QString& message);

    QQueue<FileTransferTask> pending_tasks_;
    QQueue<FileTransferTask> tasks_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferQueueBuilder);
};

} // namespace aspia

#endif // ASPIA_CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H_
