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

#ifndef ASPIA_HOST__FILE_WORKER_H_
#define ASPIA_HOST__FILE_WORKER_H_

#include "host/file_depacketizer.h"
#include "host/file_packetizer.h"
#include "host/file_request.h"
#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FileWorker : public QObject
{
    Q_OBJECT

public:
    FileWorker(QObject* parent = nullptr);
    ~FileWorker() = default;

    proto::file_transfer::Reply doRequest(const proto::file_transfer::Request& request);

public slots:
    void executeRequest(FileRequest* request);

private:
    proto::file_transfer::Reply doDriveListRequest();
    proto::file_transfer::Reply doFileListRequest(
        const proto::file_transfer::FileListRequest& request);
    proto::file_transfer::Reply doCreateDirectoryRequest(
        const proto::file_transfer::CreateDirectoryRequest& request);
    proto::file_transfer::Reply doRenameRequest(
        const proto::file_transfer::RenameRequest& request);
    proto::file_transfer::Reply doRemoveRequest(
        const proto::file_transfer::RemoveRequest& request);
    proto::file_transfer::Reply doDownloadRequest(
        const proto::file_transfer::DownloadRequest& request);
    proto::file_transfer::Reply doUploadRequest(
        const proto::file_transfer::UploadRequest& request);
    proto::file_transfer::Reply doPacketRequest();
    proto::file_transfer::Reply doPacket(const proto::file_transfer::Packet& packet);

    std::unique_ptr<FileDepacketizer> depacketizer_;
    std::unique_ptr<FilePacketizer> packetizer_;

    DISALLOW_COPY_AND_ASSIGN(FileWorker);
};

} // namespace aspia

#endif // ASPIA_HOST__FILE_WORKER_H_
