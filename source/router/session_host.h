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

#ifndef ROUTER_SESSION_HOST_H
#define ROUTER_SESSION_HOST_H

#include "base/peer/host_id.h"
#include "router/session.h"

namespace proto::router {
class ConnectionOffer;
class HostIdRequest;
} // namespace proto::router

class SessionHost final : public Session
{
    Q_OBJECT

public:
    explicit SessionHost(TcpChannel* channel, QObject* parent = nullptr);
    ~SessionHost() final;

    HostId hostId() const { return host_id_; }

    void sendConnectionOffer(const proto::router::ConnectionOffer& offer);
    // Sends the "remove" host command and marks the session so that on disconnect the
    // hosts_remove row for this host_id is finalized. TCP delivers the command reliably; the
    // host's disconnect is treated as a proof of receipt.
    void sendRemoveCommand();

signals:
    void sig_hostIdAssigned(HostId host_id);

protected:
    // Session implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    void readHostIdRequest(const proto::router::HostIdRequest& host_id_request);

    HostId host_id_ = kInvalidHostId;
    bool remove_command_sent_ = false;

    Q_DISABLE_COPY_MOVE(SessionHost)
};

#endif // ROUTER_SESSION_HOST_H
