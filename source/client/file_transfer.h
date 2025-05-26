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

#ifndef CLIENT_FILE_TRANSFER_H
#define CLIENT_FILE_TRANSFER_H

#include <QList>
#include <QMap>
#include <QQueue>
#include <QPointer>
#include <QTimer>

#include "base/location.h"
#include "common/file_task_factory.h"
#include "proto/file_transfer.h"

namespace client {

class FileTransferQueueBuilder;

class FileTransfer final : public QObject
{
    Q_OBJECT

public:
    enum class Type
    {
        DOWNLOADER,
        UPLOADER
    };

    class Error
    {
    public:
        enum class Type
        {
            UNKNOWN,
            QUEUE,
            CREATE_DIRECTORY,
            CREATE_FILE,
            OPEN_FILE,
            ALREADY_EXISTS,
            WRITE_FILE,
            READ_FILE,
            OTHER
        };

        enum Action
        {
            ACTION_ASK = 0,
            ACTION_ABORT = 1,
            ACTION_SKIP = 2,
            ACTION_SKIP_ALL = 4,
            ACTION_REPLACE = 8,
            ACTION_REPLACE_ALL = 16
        };

        Error() : type_(Type::UNKNOWN), code_(proto::FILE_ERROR_UNKNOWN)
        {
            // Nothing
        }

        Error(Type type, proto::FileError code, const QString& path)
            : type_(type),
              code_(code),
              path_(path)
        {
            // Nothing
        }

        ~Error() = default;

        Type type() const { return type_; }
        proto::FileError code() const { return code_; }
        const QString& path() const { return path_; }

        quint32 availableActions() const;
        Action defaultAction() const;

    private:
        const Type type_;
        const proto::FileError code_;
        const QString path_;
    };

    struct Item
    {
        Item(const QString& name, qint64 size, bool is_directory)
            : name(name),
              is_directory(is_directory),
              size(size)
        {
            // Nothing
        }

        Item(QString&& name, qint64 size, bool is_directory)
            : name(std::move(name)),
              is_directory(is_directory),
              size(size)
        {
            // Nothing
        }

        QString name;
        bool is_directory;
        qint64 size;
    };

    class Task
    {
    public:
        Task() = default;
        Task(QString&& source_path, QString&& target_path, bool is_directory, qint64 size);

        Task(const Task& other) = default;
        Task& operator=(const Task& other) = default;

        Task(Task&& other) noexcept;
        Task& operator=(Task&& other) noexcept;

        ~Task() = default;

        const QString& sourcePath() const { return source_path_; }
        const QString& targetPath() const { return target_path_; }
        bool isDirectory() const { return is_directory_; }
        qint64 size() const { return size_; }

        bool overwrite() const { return overwrite_; }
        void setOverwrite(bool value) { overwrite_ = value; }

    private:
        QString source_path_;
        QString target_path_;
        bool is_directory_;
        bool overwrite_ = false;
        qint64 size_;
    };

    using TaskList = QQueue<Task>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    FileTransfer(Type type,
                 const QString& source_path,
                 const QString& target_path,
                 const QList<Item>& items,
                 QObject* parent = nullptr);
    ~FileTransfer() final;

    void start();
    void stop();

    void setAction(client::FileTransfer::Error::Type error_type,
                   client::FileTransfer::Error::Action action);

signals:
    void sig_started();
    void sig_finished();
    void sig_progressChanged(int total, int current);
    void sig_currentItemChanged(const QString& source_path, const QString& target_path);
    void sig_currentSpeedChanged(qint64 speed);
    void sig_errorOccurred(const client::FileTransfer::Error& error);
    void sig_doTask(const common::FileTask& task);

private slots:
    void onTaskDone(const common::FileTask& task);

private:
    Task& frontTask();
    void targetReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void sourceReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void doFrontTask(bool overwrite);
    void doNextTask();
    void doUpdateSpeed();
    void onError(Error::Type type, proto::FileError code, const QString& path = QString());
    void setActionForErrorType(Error::Type error_type, Error::Action action);
    void onFinished(const base::Location& location);

    const Type type_;
    const QString source_path_;
    const QString target_path_;
    const QList<Item> items_;

    QPointer<common::FileTaskFactory> task_factory_source_;
    QPointer<common::FileTaskFactory> task_factory_target_;

    QPointer<QTimer> cancel_timer_ = nullptr;

    // The map contains available actions for the error and the current action.
    QMap<Error::Type, Error::Action> actions_;
    QPointer<FileTransferQueueBuilder> queue_builder_;
    TaskList tasks_;

    qint64 total_size_ = 0;
    qint64 total_transfered_size_ = 0;
    qint64 task_transfered_size_ = 0;

    int total_percentage_ = 0;
    int task_percentage_ = 0;

    bool is_canceled_ = false;

    QPointer<QTimer> speed_update_timer_;
    TimePoint begin_time_;
    qint64 bytes_per_time_ = 0;
    qint64 speed_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileTransfer);
};

} // namespace client

Q_DECLARE_METATYPE(client::FileTransfer::Error::Action)
Q_DECLARE_METATYPE(client::FileTransfer::Error::Type)
Q_DECLARE_METATYPE(client::FileTransfer::Error)

#endif // CLIENT_FILE_TRANSFER_H
