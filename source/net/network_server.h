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

#ifndef NET__NETWORK_SERVER_H
#define NET__NETWORK_SERVER_H

#include "base/macros_magic.h"
#include "net/srp_user.h"

#include <QPointer>
#include <QList>
#include <QTcpServer>

namespace net {

class ChannelHost;

class Server : public QObject
{
    Q_OBJECT

public:
    Server(const SrpUserList& user_list, QObject* parent = nullptr);
    ~Server() = default;

    bool start(uint16_t port);
    void stop();

    bool hasReadyChannels() const;
    ChannelHost* nextReadyChannel();

signals:
    void newChannelReady();

private slots:
    void onNewConnection();
    void onChannelReady();

private:
    QPointer<QTcpServer> tcp_server_;
    SrpUserList user_list_;

    // Contains a list of channels that are already connected, but the key exchange
    // is not yet complete.
    QList<QPointer<ChannelHost>> pending_channels_;

    // Contains a list of channels that are ready for use.
    QList<QPointer<ChannelHost>> ready_channels_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace net

#endif // NET__NETWORK_SERVER_H
