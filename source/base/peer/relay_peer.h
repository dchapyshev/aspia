//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QObject>

#include "base/logging.h"
#include "base/net/proxy_settings.h"
#include "base/net/tcp_channel.h"
#include "proto/router_peer.h"

namespace proto::router {
class RelayKey;
} // namespace proto::router

class Authenticator;

class RelayPeer final : public QObject
{
    Q_OBJECT

public:
    explicit RelayPeer(Authenticator* authenticator, QObject* parent = nullptr);
    ~RelayPeer();

    // Connects to the relay from |offer| through the proxy with |proxy|, or directly without it.
    void start(const proto::router::ConnectionOffer& offer, const ProxySettings& proxy);
    bool isFinished() const { return is_finished_; }
    const proto::router::ConnectionOffer& connectionOffer() const { return connection_offer_; }

    TcpChannel* takeChannel();
    bool hasChannel() const;

signals:
    void sig_connectionReady();
    // |relay_connected| is true when the relay was reached and the peer authentication failed, and
    // false when the relay itself (or the proxy on the way to it) could not be reached.
    void sig_connectionError(TcpChannel::ErrorCode error_code, bool relay_connected);

private:
    void onErrorOccurred(TcpChannel::ErrorCode error_code);

    QByteArray authenticationMessage(const proto::router::RelayKey& key, const std::string& secret);

    std::unique_ptr<Authenticator> authenticator_;
    proto::router::ConnectionOffer connection_offer_;
    bool is_finished_ = false;

    // Set once the relay (through the proxy, if any) accepted the connection. The channel resets its own
    // connected state before it reports an error, so the peer keeps this one.
    bool relay_connected_ = false;

    TcpChannel* pending_channel_ = nullptr;

    LOG_DECLARE_CONTEXT(RelayPeer);
    Q_DISABLE_COPY_MOVE(RelayPeer)
};

#endif // BASE_PEER_RELAY_PEER_H
