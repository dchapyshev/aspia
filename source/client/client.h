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

#ifndef ASPIA_CLIENT__CLIENT_H_
#define ASPIA_CLIENT__CLIENT_H_

#include "client/client_session.h"
#include "client/connect_data.h"
#include "network/network_channel.h"

namespace aspia {

class ClientUserAuthorizer;
class StatusDialog;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const ConnectData& connect_data, QObject* parent = nullptr);
    ~Client() = default;

signals:
    void clientTerminated(Client* client);

private slots:
    void onChannelConnected();
    void onChannelDisconnected();
    void onChannelError(const QString& message);
    void onAuthorizationFinished(proto::auth::Status status);
    void onSessionClosedByUser();
    void onSessionError(const QString& message);

private:
    ConnectData connect_data_;

    QPointer<NetworkChannel> network_channel_;
    QPointer<StatusDialog> status_dialog_;
    QPointer<ClientUserAuthorizer> authorizer_;
    QPointer<ClientSession> session_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_H_
