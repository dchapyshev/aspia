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

#ifndef COMMON__FILE_WORKER_H
#define COMMON__FILE_WORKER_H

#include "common/file_depacketizer.h"
#include "common/file_packetizer.h"
#include "common/file_request.h"
#include "proto/file_transfer.pb.h"

namespace common {

class FileWorker : public QObject
{
    Q_OBJECT

public:
    FileWorker(QObject* parent = nullptr);
    ~FileWorker() = default;

    proto::FileReply doRequest(const proto::FileRequest& request);

public slots:
    void executeRequest(FileRequest* request);

private:
    proto::FileReply doDriveListRequest();
    proto::FileReply doFileListRequest(const proto::FileListRequest& request);
    proto::FileReply doCreateDirectoryRequest(const proto::CreateDirectoryRequest& request);
    proto::FileReply doRenameRequest(const proto::RenameRequest& request);
    proto::FileReply doRemoveRequest(const proto::RemoveRequest& request);
    proto::FileReply doDownloadRequest(const proto::DownloadRequest& request);
    proto::FileReply doUploadRequest(const proto::UploadRequest& request);
    proto::FileReply doPacketRequest(const proto::FilePacketRequest& request);
    proto::FileReply doPacket(const proto::FilePacket& packet);

    std::unique_ptr<FileDepacketizer> depacketizer_;
    std::unique_ptr<FilePacketizer> packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileWorker);
};

} // namespace common

#endif // COMMON__FILE_WORKER_H
