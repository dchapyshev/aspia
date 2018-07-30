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

#ifndef ASPIA_CLIENT__FILE_REMOVER_H_
#define ASPIA_CLIENT__FILE_REMOVER_H_

#include <QQueue>

#include "client/file_remove_task.h"
#include "host/file_request.h"
#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FileRemoveQueueBuilder;

class FileRemover : public QObject
{
    Q_OBJECT

public:
    explicit FileRemover(QObject* parent = nullptr);
    ~FileRemover() = default;

    enum Action
    {
        Ask     = 0,
        Abort   = 1,
        Skip    = 2,
        SkipAll = 4
    };
    Q_DECLARE_FLAGS(Actions, Action)

    struct Item
    {
        Item(const QString& name, bool is_directory)
            : name(name),
              is_directory(is_directory)
        {
            // Nothing
        }

        QString name;
        bool is_directory;
    };

    void start(const QString& path, const QList<Item>& items);

signals:
    void started();
    void finished();
    void progressChanged(const QString& name, int percentage);
    void error(FileRemover* remover, FileRemover::Actions actions, const QString& message);
    void newRequest(FileRequest* request);

public slots:
    void applyAction(Action action);
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);

private slots:
    void taskQueueError(const QString& message);
    void taskQueueReady();

private:
    void processTask();
    void processNextTask();

    FileRemoveQueueBuilder* builder_ = nullptr;
    QQueue<FileRemoveTask> tasks_;

    Action failure_action_ = Ask;
    int tasks_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileRemover);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileRemover::Actions)

} // namespace aspia

#endif // ASPIA_CLIENT__FILE_REMOVER_H_
