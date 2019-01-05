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

#ifndef ASPIA_NETWORK__NETWORK_CHANNEL_HOST_H
#define ASPIA_NETWORK__NETWORK_CHANNEL_HOST_H

#include "network/network_channel.h"
#include "network/srp_user.h"
#include "protocol/common.pb.h"

namespace aspia {

class SrpHostContext;

class NetworkChannelHost : public NetworkChannel
{
    Q_OBJECT

public:
    ~NetworkChannelHost();

    void startKeyExchange();

    const QString& userName() const { return username_; }
    proto::SessionType sessionType() const { return session_type_; }

signals:
    void keyExchangeFinished();

protected:
    friend class NetworkServer;
    NetworkChannelHost(QTcpSocket* socket,
                       const SrpUserList& user_list,
                       QObject* parent = nullptr);

    // NetworkChannel implementation.
    void internalMessageReceived(const QByteArray& buffer) override;
    void internalMessageWritten() override;

private:
    void readClientHello(const QByteArray& buffer);
    void readIdentify(const QByteArray& buffer);
    void readClientKeyExchange(const QByteArray& buffer);
    void readSessionResponse(const QByteArray& buffer);

    SrpUserList user_list_;

    QString username_;
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;

    std::unique_ptr<SrpHostContext> srp_host_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelHost);
};

} // namespace aspia

#endif // ASPIA_NETWORK__NETWORK_CHANNEL_HOST_H
