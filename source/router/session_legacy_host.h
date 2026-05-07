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

#ifndef ROUTER_SESSION_LEGACY_HOST_H
#define ROUTER_SESSION_LEGACY_HOST_H

#include "base/peer/host_id.h"
#include "router/session.h"

namespace proto::router::legacy {
class ConnectionOffer;
class HostIdRequest;
class ResetHostId;
} // namespace proto::router::legacy

class SessionLegacyHost final : public Session
{
    Q_OBJECT

public:
    explicit SessionLegacyHost(TcpChannel* channel, QObject* parent = nullptr);
    ~SessionLegacyHost() final;

    const QList<HostId>& hostIdList() const { return host_id_list_; }
    bool hasHostId(HostId host_id) const;

    void sendConnectionOffer(const proto::router::legacy::ConnectionOffer& offer);

signals:
    void sig_hostIdAssigned(HostId host_id);

protected:
    // Session implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    void readHostIdRequest(const proto::router::legacy::HostIdRequest& host_id_request);
    void readResetHostId(const proto::router::legacy::ResetHostId& reset_host_id);

    QList<HostId> host_id_list_;

    Q_DISABLE_COPY_MOVE(SessionLegacyHost)
};

#endif // ROUTER_SESSION_LEGACY_HOST_H
