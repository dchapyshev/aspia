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

#ifndef ASPIA_NET__NETWORK_CHANNEL_CLIENT_H
#define ASPIA_NET__NETWORK_CHANNEL_CLIENT_H

#include <QVersionNumber>

#include "net/network_channel.h"
#include "protocol/common.pb.h"

namespace net {

class SrpClientContext;

class ChannelClient : public Channel
{
    Q_OBJECT

public:
    ChannelClient(QObject* parent = nullptr);
    ~ChannelClient();

    // Connection to the host.
    void connectToHost(const QString& address, int port,
                       const QString& username, const QString& password,
                       proto::SessionType session_type);

signals:
    // Emits when a secure connection is established.
    void connected();

protected:
    // Channel implementation.
    void internalMessageReceived(const QByteArray& buffer) override;
    void internalMessageWritten() override;

private slots:
    void onConnected();

private:
    void readServerHello(const QByteArray& buffer);
    void readServerKeyExchange(const QByteArray& buffer);
    void readSessionChallenge(const QByteArray& buffer);

    QString username_;
    QString password_;
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;

    std::unique_ptr<SrpClientContext> srp_client_;

    DISALLOW_COPY_AND_ASSIGN(ChannelClient);
};

} // namespace net

#endif // ASPIA_NET__NETWORK_CHANNEL_CLIENT_H
