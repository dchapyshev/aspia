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

#ifndef COMMON__FILE_REQUEST_H
#define COMMON__FILE_REQUEST_H

#include "base/macros_magic.h"
#include "proto/file_transfer.pb.h"

#include <QObject>
#include <QPointer>
#include <QString>

namespace common {

class FileRequest : public QObject
{
    Q_OBJECT

public:
    const proto::FileRequest& request() const { return request_; }
    void sendReply(const proto::FileReply& reply);

    static FileRequest* driveListRequest();
    static FileRequest* fileListRequest(const QString& path);
    static FileRequest* createDirectoryRequest(const QString& path);
    static FileRequest* renameRequest(const QString& old_name, const QString& new_name);
    static FileRequest* removeRequest(const QString& path);
    static FileRequest* downloadRequest(const QString& file_path);
    static FileRequest* uploadRequest(const QString& file_path, bool overwrite);
    static FileRequest* packetRequest(uint32_t flags);
    static FileRequest* packet(const proto::FilePacket& packet);

signals:
    void replyReady(const proto::FileRequest& request, const proto::FileReply& reply);

private:
    FileRequest(proto::FileRequest&& request);

    proto::FileRequest request_;

    DISALLOW_COPY_AND_ASSIGN(FileRequest);
};

} // namespace common

#endif // COMMON__FILE_REQUEST_H
