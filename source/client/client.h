//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__CLIENT_H
#define ASPIA_CLIENT__CLIENT_H

#include <QObject>
#include <QPointer>

#include "base/macros_magic.h"
#include "client/connect_data.h"
#include "network/network_channel_client.h"

namespace aspia {

class NetworkChannel;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const ConnectData& connect_data, QObject* parent);
    virtual ~Client();

    // Starts session.
    void start();

    ConnectData& connectData() { return connect_data_; }

    // Returns the version of the connected host.
    QVersionNumber hostVersion() const;

signals:
    // Indicates that the session is started.
    void started();

    // Indicates that the session is finished.
    void finished();

    // Indicates an error in the session.
    void errorOccurred(const QString& message);

protected:
    // Reads the incoming message for the session.
    virtual void messageReceived(const QByteArray& buffer) = 0;

    // Sends outgoing message.
    void sendMessage(const google::protobuf::MessageLite& message);

private:
    static QString networkErrorToString(NetworkChannel::Error error);

    ConnectData connect_data_;
    QPointer<NetworkChannelClient> network_channel_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_H
