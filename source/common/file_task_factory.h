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

#include <QString>

#include "base/macros_magic.h"
#include "common/file_task.h"

namespace proto::file_transfer {
class FilePacket;
} // namespace proto::file_transfer

namespace common {

class FileTaskFactory final : public QObject
{
    Q_OBJECT

public:
    explicit FileTaskFactory(FileTask::Target target, QObject* parent = nullptr);
    ~FileTaskFactory();

    FileTask::Target target() const { return target_; }

    FileTask driveList();
    FileTask fileList(const QString& path);
    FileTask createDirectory(const QString& path);
    FileTask rename(const QString& old_name, const QString& new_name);
    FileTask remove(const QString& path);
    FileTask download(const QString& file_path);
    FileTask upload(const QString& file_path, bool overwrite);
    FileTask packetRequest(quint32 flags);
    FileTask packet(const proto::file_transfer::Packet& packet);
    FileTask packet(proto::file_transfer::Packet&& packet);

signals:
    void sig_taskDone(const common::FileTask& task);

private:
    FileTask makeTask(proto::file_transfer::Request&& request);

    const FileTask::Target target_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskFactory);
};

} // namespace common

#endif // CLIENT_FILE_TASK_FACTORY_H
