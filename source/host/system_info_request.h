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

#ifndef _ASPIA_HOST__SYSTEM_INFO_REQUEST_H
#define _ASPIA_HOST__SYSTEM_INFO_REQUEST_H

#include <QObject>
#include <QString>

#include "protocol/system_info_session.pb.h"

namespace aspia {

class SystemInfoRequest : public QObject
{
    Q_OBJECT

public:
    const proto::system_info::Request& request() const { return request_; }
    const std::string& requestUuid() const { return request_.request_uuid(); }

    static SystemInfoRequest* categoryList();
    static SystemInfoRequest* category(const QString& uuid, const QByteArray& params);

signals:
    void replyReady(const proto::system_info::Reply& reply);

private:
    SystemInfoRequest(proto::system_info::Request&& request);

    proto::system_info::Request request_;

    Q_DISABLE_COPY(SystemInfoRequest)
};

} // namespace aspia

#endif // _ASPIA_HOST__SYSTEM_INFO_REQUEST_H
