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

#ifndef ROUTER_CLIENT_H
#define ROUTER_CLIENT_H

#include <QObject>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"

namespace proto::router {
class ChangePasswordRequest;
class CheckHostStatus;
class ConnectionRequest;
class GroupListRequest;
class HostListRequest;
class HostSearchRequest;
class TempHostListRequest;
class TwoFactorResponse;
class WorkspaceListRequest;
enum SessionType : int;
} // namespace proto::router

class Host;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(TcpChannel* channel, QObject* parent);
    virtual ~Client() override;

    void start();

    QVersionNumber version() const;
    const std::string& osName() const;
    const std::string& computerName() const;
    const std::string& architecture() const;
    const std::string& userName() const;
    qint64 userId() const;
    proto::router::SessionType sessionType() const;

    qint64 sessionId() const { return session_id_; }
    const std::string& address() const { return tcp_channel_->peerAddress(); }
    time_t startTime() const { return start_time_; }

    void sendMessage(quint8 channel_id, const QByteArray& message);

    void setStunInfo(quint16 port);

    bool isTwoFactorCompleted() const { return two_factor_completed_; }

    // Router-side row id of the device token this session authenticated with (0 until 2FA
    // completes). Lets the admin channel tear down the live connection when its token is revoked.
    qint64 tokenId() const { return token_id_; }

signals:
    void sig_started(qint64 session_id);
    void sig_finished(qint64 session_id);

protected:
    LOG_DECLARE_CONTEXT(Client);

    virtual void onSessionMessage(quint8 channel_id, const QByteArray& buffer);

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onStarted();

private:
    void doTwoFactorChallenge();
    void readTwoFactorResponse(const proto::router::TwoFactorResponse& response);
    void completeTwoFactor(std::string&& new_token = std::string());
    void sendUserKeys();
    void readConnectionRequest(const proto::router::ConnectionRequest& request);
    void readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status);
    void readHostListRequest(const proto::router::HostListRequest& request);
    void readHostSearchRequest(const proto::router::HostSearchRequest& request);
    void readTempHostListRequest(const proto::router::TempHostListRequest& request);
    void readWorkspaceListRequest(const proto::router::WorkspaceListRequest& request);
    void readGroupListRequest(const proto::router::GroupListRequest& request);
    void readChangePasswordRequest(const proto::router::ChangePasswordRequest& request);
    Host* hostByHostId(HostId host_id);

    const qint64 session_id_;
    time_t start_time_ = 0;

    TcpChannel* tcp_channel_ = nullptr;
    quint16 stun_port_ = 0;

    bool two_factor_completed_ = false;
    qint64 token_id_ = 0;
    QByteArray tentative_otp_secret_;
    QByteArray user_otp_secret_;
    quint64 user_otp_counter_ = 0;

    Q_DISABLE_COPY_MOVE(Client)
};

#endif // ROUTER_CLIENT_H
