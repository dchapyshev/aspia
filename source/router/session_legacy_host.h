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
#include "proto/router_legacy_host.h"
#include "router/session.h"

namespace router {

class SessionLegacyHost final : public Session
{
    Q_OBJECT

public:
    explicit SessionLegacyHost(base::TcpChannel* channel, QObject* parent = nullptr);
    ~SessionLegacyHost() final;

    const QList<base::HostId>& hostIdList() const { return host_id_list_; }
    bool hasHostId(base::HostId host_id) const;

    void sendConnectionOffer(const proto::router::legacy::ConnectionOffer& offer);

signals:
    void sig_hostIdAssigned(base::HostId host_id);

protected:
    // Session implementation.
    void onSessionMessage(const QByteArray& buffer) final;

private:
    void readHostIdRequest(const proto::router::legacy::HostIdRequest& host_id_request);
    void readResetHostId(const proto::router::legacy::ResetHostId& reset_host_id);

    QList<base::HostId> host_id_list_;

    Q_DISABLE_COPY_MOVE(SessionLegacyHost)
};

} // namespace router

#endif // ROUTER_SESSION_LEGACY_HOST_H
