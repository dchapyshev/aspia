//
// PROJECT:         Aspia
// FILE:            client/file_request.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_H
#define _ASPIA_CLIENT__FILE_REQUEST_H

#include <QString>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileRequest
{
public:
    static proto::file_transfer::Request driveListRequest();
    static proto::file_transfer::Request fileListRequest(const QString& path);
    static proto::file_transfer::Request createDirectoryRequest(const QString& path);
    static proto::file_transfer::Request renameRequest(const QString& old_name, const QString& new_name);
    static proto::file_transfer::Request removeRequest(const QString& path);
    static proto::file_transfer::Request downloadRequest(const QString& file_path);
    static proto::file_transfer::Request uploadRequest(const QString& file_path, bool overwrite);
    static proto::file_transfer::Request packetRequest();
    static proto::file_transfer::Request packet(const proto::file_transfer::Packet& packet);

private:
    DISALLOW_COPY_AND_ASSIGN(FileRequest);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_H
