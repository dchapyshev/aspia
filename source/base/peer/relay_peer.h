//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_PEER_RELAY_PEER_H
#define BASE_PEER_RELAY_PEER_H

#include <QByteArray>
#include <QObject>

#include "base/location.h"
#include "base/net/tcp_channel.h"
#include "proto/router.h"
#include "proto/router_peer.h"

#include <asio/ip/tcp.hpp>

namespace base {

class RelayPeer final : public QObject
{
    Q_OBJECT

public:
    explicit RelayPeer(QObject* parent = nullptr);
    ~RelayPeer();

    void start(const proto::router::ConnectionOffer& offer);
    bool isFinished() const { return is_finished_; }
    const proto::router::ConnectionOffer& connectionOffer() const { return connection_offer_; }

    TcpChannel* takeChannel();
    bool hasChannel() const;

signals:
    void sig_connectionReady();
    void sig_connectionError();

private:
    void onConnected();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);

    static QByteArray authenticationMessage(
        const proto::router::RelayKey& key, const std::string& secret);

    proto::router::ConnectionOffer connection_offer_;
    bool is_finished_ = false;

    quint32 message_size_ = 0;
    QByteArray message_;

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;

    TcpChannel* pending_channel_ = nullptr;

    Q_DISABLE_COPY(RelayPeer)
};

} // namespace base

#endif // BASE_PEER_RELAY_PEER_H
