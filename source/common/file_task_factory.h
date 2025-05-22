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

#ifndef CLIENT_FILE_TASK_FACTORY_H
#define CLIENT_FILE_TASK_FACTORY_H

#include "common/file_task.h"

#include <QString>

namespace proto {
class FilePacket;
} // namespace proto

namespace common {

class FileTaskFactory final : public QObject
{
    Q_OBJECT

public:
    explicit FileTaskFactory(FileTask::Target target, QObject* parent = nullptr);
    ~FileTaskFactory();

    FileTask::Target target() const { return target_; }

    base::local_shared_ptr<FileTask> driveList();
    base::local_shared_ptr<FileTask> fileList(const QString& path);
    base::local_shared_ptr<FileTask> createDirectory(const QString& path);
    base::local_shared_ptr<FileTask> rename(const QString& old_name, const QString& new_name);
    base::local_shared_ptr<FileTask> remove(const QString& path);
    base::local_shared_ptr<FileTask> download(const QString& file_path);
    base::local_shared_ptr<FileTask> upload(const QString& file_path, bool overwrite);
    base::local_shared_ptr<FileTask> packetRequest(quint32 flags);
    base::local_shared_ptr<FileTask> packet(const proto::FilePacket& packet);
    base::local_shared_ptr<FileTask> packet(std::unique_ptr<proto::FilePacket> packet);

signals:
    void sig_taskDone(base::local_shared_ptr<FileTask> task);

private:
    base::local_shared_ptr<FileTask> makeTask(std::unique_ptr<proto::FileRequest> request);

    const FileTask::Target target_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskFactory);
};

} // namespace common

#endif // CLIENT_FILE_TASK_FACTORY_H
