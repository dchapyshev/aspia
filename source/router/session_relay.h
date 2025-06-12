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

#ifndef ROUTER_SESSION_RELAY_H
#define ROUTER_SESSION_RELAY_H

#include "base/serialization.h"
#include "proto/router_relay.h"
#include "router/session.h"

namespace router {

class SessionRelay final : public Session
{
public:
    explicit SessionRelay(QObject* parent = nullptr);
    ~SessionRelay() final;

    using PeerData = std::pair<std::string, quint16>;

    const std::optional<PeerData>& peerData() const { return peer_data_; }
    const std::optional<proto::router::RelayStat>& relayStat() const { return relay_stat_; }
    void sendKeyUsed(quint32 key_id);
    void disconnectPeerSession(const proto::router::PeerConnectionRequest& request);

protected:
    // Session implementation.
    void onSessionReady() final;
    void onSessionMessageReceived(quint8 channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(quint8 channel_id, size_t pending) final;

private:
    void readKeyPool(const proto::router::RelayKeyPool& key_pool);

    std::optional<PeerData> peer_data_;
    std::optional<proto::router::RelayStat> relay_stat_;

    base::Parser<proto::router::RelayToRouter> incoming_message_;
    base::Serializer<proto::router::RouterToRelay> outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(SessionRelay);
};

} // namespace router

#endif // ROUTER_SESSION_RELAY_H
