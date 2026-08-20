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

#ifndef ROUTER_CLIENT_OPERATOR_H
#define ROUTER_CLIENT_OPERATOR_H

#include <QObject>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "router/handlers/request_caller.h"
#include "router/handlers/two_factor_handler.h"

class Database;
struct RequestResult;

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

class ClientOperator : public QObject
{
    Q_OBJECT

public:
    // |database| is the connection of the thread the session runs in; it outlives every session
    // of that thread. Passing it instead of reaching for the per-thread singleton is what lets a
    // session be driven against a temporary database in a test.
    ClientOperator(Database& database, TcpChannel* channel, QObject* parent);
    virtual ~ClientOperator() override;

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
    void sig_notifyChanged(quint32 flags);
    void sig_stopClients(qint64 user_id, const std::vector<qint64>& token_ids, qint64 except_client_id);

protected:
    LOG_DECLARE_CONTEXT(ClientOperator);

    Database& database() const { return database_; }

    virtual void onSessionMessage(quint8 channel_id, const QByteArray& buffer);

    // The identity the request handlers work with. Taken from the authenticated channel, never
    // from the request itself.
    RequestCaller requestCaller() const;

    // Turns the side effects a command handler returned into signals. Called after the reply is
    // sent: the sessions being stopped can include the one that sent the request (an administrator
    // disabling its own account), and it must still see the result of its command.
    void applyRequestResult(const RequestResult& result);

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onStarted();

private:
    void doTwoFactorChallenge();
    void readTwoFactorResponse(const proto::router::TwoFactorResponse& response);

    // Sends what the two-factor stage decided: a challenge, the completion of the stage, or the
    // teardown of the session.
    void applyTwoFactorResult(TwoFactorHandler::Result&& result);
    void completeTwoFactor(std::string&& new_token = std::string());
    void readConnectionRequest(const proto::router::ConnectionRequest& request);
    void sendConnectionOffer(qint64 request_id, HostId host_id);
    void readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status);
    void readHostListRequest(const proto::router::HostListRequest& request);
    void readHostSearchRequest(const proto::router::HostSearchRequest& request);
    void readTempHostListRequest(const proto::router::TempHostListRequest& request);
    void readWorkspaceListRequest(const proto::router::WorkspaceListRequest& request);
    void readGroupListRequest(const proto::router::GroupListRequest& request);
    void readChangePasswordRequest(const proto::router::ChangePasswordRequest& request);

    Database& database_;
    const qint64 session_id_;
    time_t start_time_ = 0;

    TcpChannel* tcp_channel_ = nullptr;
    quint16 stun_port_ = 0;

    TwoFactorHandler two_factor_;
    bool two_factor_completed_ = false;
    qint64 token_id_ = 0;

    Q_DISABLE_COPY_MOVE(ClientOperator)
};

#endif // ROUTER_CLIENT_OPERATOR_H
