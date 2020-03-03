//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__FILE_TASK_FACTORY_H
#define CLIENT__FILE_TASK_FACTORY_H

#include "common/file_task.h"

#include <string>

namespace proto {
class FilePacket;
} // namespace proto

namespace common {

class FileTaskFactory
{
public:
    FileTaskFactory(std::shared_ptr<FileTaskProducerProxy> producer_proxy, FileTask::Target target);
    ~FileTaskFactory();

    FileTask::Target target() const { return target_; }

    std::shared_ptr<FileTask> driveList();
    std::shared_ptr<FileTask> fileList(const std::string& path);
    std::shared_ptr<FileTask> createDirectory(const std::string& path);
    std::shared_ptr<FileTask> rename(const std::string& old_name, const std::string& new_name);
    std::shared_ptr<FileTask> remove(const std::string& path);
    std::shared_ptr<FileTask> download(const std::string& file_path);
    std::shared_ptr<FileTask> upload(const std::string& file_path, bool overwrite);
    std::shared_ptr<FileTask> packetRequest(uint32_t flags);
    std::shared_ptr<FileTask> packet(const proto::FilePacket& packet);
    std::shared_ptr<FileTask> packet(std::unique_ptr<proto::FilePacket> packet);

private:
    std::shared_ptr<FileTask> makeTask(std::unique_ptr<proto::FileRequest> request);

    std::shared_ptr<FileTaskProducerProxy> producer_proxy_;
    const FileTask::Target target_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskFactory);
};

} // namespace common

#endif // CLIENT__FILE_TASK_FACTORY_H
