//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_FILE_REMOVER_H
#define CLIENT_FILE_REMOVER_H

#include <QObject>
#include <QPointer>
#include <QQueue>

#include "base/location.h"
#include "common/file_task_factory.h"
#include "proto/file_transfer.h"

namespace client {

class FileRemoveQueueBuilder;

class FileRemover final : public QObject
{
    Q_OBJECT

public:
    enum Action
    {
        ACTION_ASK      = 0,
        ACTION_ABORT    = 1,
        ACTION_SKIP     = 2,
        ACTION_SKIP_ALL = 4
    };

    class Task
    {
    public:
        Task(QString&& path, bool is_directory);

        Task(const Task& other) = default;
        Task& operator=(const Task& other) = default;

        Task(Task&& other) noexcept;
        Task& operator=(Task&& other) noexcept;

        ~Task() = default;

        const QString& path() const { return path_; }
        bool isDirectory() const { return is_directory_; }

    private:
        QString path_;
        bool is_directory_;
    };


    using TaskList = QQueue<Task>;

    FileRemover(common::FileTask::Target target, const TaskList& items, QObject* parent = nullptr);
    ~FileRemover() final;

public slots:
    void start();
    void stop();
    void setAction(Action action);

signals:
    void sig_started();
    void sig_finished();
    void sig_progressChanged(const QString& name, int percentage);
    void sig_errorOccurred(const QString& path,
                           proto::file_transfer::ErrorCode error_code,
                           quint32 available_actions);
    void sig_doTask(const common::FileTask& task);

private slots:
    void onTaskDone(const common::FileTask& task);

private:
    void doNextTask();
    void doCurrentTask();
    void onFinished(const base::Location& location);

    QPointer<common::FileTaskFactory> task_factory_;
    QPointer<FileRemoveQueueBuilder> queue_builder_;

    TaskList tasks_;

    Action failure_action_ = ACTION_ASK;
    size_t tasks_count_ = 0;

    Q_DISABLE_COPY(FileRemover)
};

} // namespace client

Q_DECLARE_METATYPE(client::FileRemover::Action)

#endif // CLIENT_FILE_REMOVER_H
