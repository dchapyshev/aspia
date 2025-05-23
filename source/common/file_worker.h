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

#include "base/macros_magic.h"
#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_task.h"

#include <memory>

namespace common {

class FileWorker
{
public:
    FileWorker();
    ~FileWorker();

    void doRequest(const common::FileTask& task);
    void doRequest(const proto::FileRequest& request, proto::FileReply* reply);

private:
    void doDriveListRequest(proto::FileReply* reply);
    void doFileListRequest(const proto::FileListRequest& request, proto::FileReply* reply);
    void doCreateDirectoryRequest(const proto::CreateDirectoryRequest& request, proto::FileReply* reply);
    void doRenameRequest(const proto::RenameRequest& request, proto::FileReply* reply);
    void doRemoveRequest(const proto::RemoveRequest& request, proto::FileReply* reply);
    void doDownloadRequest(const proto::DownloadRequest& request, proto::FileReply* reply);
    void doUploadRequest(const proto::UploadRequest& request, proto::FileReply* reply);
    void doPacketRequest(const proto::FilePacketRequest& request, proto::FileReply* reply);
    void doPacket(const proto::FilePacket& packet, proto::FileReply* reply);

    std::unique_ptr<FileDepacketizer> depacketizer_;
    std::unique_ptr<FilePacketizer> packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileWorker);
};

} // namespace common

#endif // COMMON_FILE_WORKER_IMPL_H
