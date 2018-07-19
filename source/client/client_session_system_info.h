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

#ifndef ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H_
#define ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H_

#include "client/client_session.h"
#include "client/connect_data.h"
#include "host/system_info_request.h"
#include "protocol/system_info_session.pb.h"

namespace aspia {

Q_DECLARE_METATYPE(proto::system_info::Request);
Q_DECLARE_METATYPE(proto::system_info::Reply);

class SystemInfoWindow;

class ClientSessionSystemInfo : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionSystemInfo(ConnectData* connect_data, QObject* parent);
    ~ClientSessionSystemInfo() = default;

public slots:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;
    void startSession() override;
    void closeSession() override;

private slots:
    void onNewRequest(SystemInfoRequest* request);

private:
    void writeNextRequest();

    ConnectData* connect_data_;

    std::unique_ptr<SystemInfoWindow> window_;
    std::list<std::unique_ptr<SystemInfoRequest>> requests_;

    Q_DISABLE_COPY(ClientSessionSystemInfo)
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H_
