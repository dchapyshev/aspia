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

#ifndef COMMON_FILE_WORKER_H
#define COMMON_FILE_WORKER_H

#include <QObject>

#include "base/macros_magic.h"
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_task.h"

#include <memory>

namespace common {

class FileWorker final : public QObject
{
    Q_OBJECT

public:
    explicit FileWorker(QObject* parent = nullptr);
    ~FileWorker();

    void doRequest(const common::FileTask& task);
    void doRequest(const proto::file_transfer::Request& request, proto::file_transfer::Reply* reply);

private:
    void doDriveListRequest(proto::file_transfer::Reply* reply);
    void doFileListRequest(const proto::file_transfer::ListRequest& request, proto::file_transfer::Reply* reply);
    void doCreateDirectoryRequest(const proto::file_transfer::CreateDirectoryRequest& request, proto::file_transfer::Reply* reply);
    void doRenameRequest(const proto::file_transfer::RenameRequest& request, proto::file_transfer::Reply* reply);
    void doRemoveRequest(const proto::file_transfer::RemoveRequest& request, proto::file_transfer::Reply* reply);
    void doDownloadRequest(const proto::file_transfer::DownloadRequest& request, proto::file_transfer::Reply* reply);
    void doUploadRequest(const proto::file_transfer::UploadRequest& request, proto::file_transfer::Reply* reply);
    void doPacketRequest(const proto::file_transfer::PacketRequest& request, proto::file_transfer::Reply* reply);
    void doPacket(const proto::file_transfer::Packet& packet, proto::file_transfer::Reply* reply);

    QTimer* idle_timer_ = nullptr;

    std::unique_ptr<FileDepacketizer> depacketizer_;
    std::unique_ptr<FilePacketizer> packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileWorker);
};

} // namespace common

#endif // COMMON_FILE_WORKER_IMPL_H
