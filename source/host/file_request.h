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

#ifndef ASPIA_HOST__FILE_REQUEST_H_
#define ASPIA_HOST__FILE_REQUEST_H_

#include <QObject>
#include <QPointer>
#include <QString>

#include "base/macros_magic.h"
#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FileRequest : public QObject
{
    Q_OBJECT

public:
    const proto::file_transfer::Request& request() const { return request_; }
    void sendReply(const proto::file_transfer::Reply& reply);

    static FileRequest* driveListRequest();
    static FileRequest* fileListRequest(const QString& path);
    static FileRequest* createDirectoryRequest(const QString& path);
    static FileRequest* renameRequest(const QString& old_name, const QString& new_name);
    static FileRequest* removeRequest(const QString& path);
    static FileRequest* downloadRequest(const QString& file_path);
    static FileRequest* uploadRequest(const QString& file_path, bool overwrite);
    static FileRequest* packetRequest();
    static FileRequest* packet(const proto::file_transfer::Packet& packet);

signals:
    void replyReady(const proto::file_transfer::Request& request,
                    const proto::file_transfer::Reply& reply);

private:
    FileRequest(proto::file_transfer::Request&& request);

    proto::file_transfer::Request request_;

    DISALLOW_COPY_AND_ASSIGN(FileRequest);
};

} // namespace aspia

#endif // ASPIA_HOST__FILE_REQUEST_H_
