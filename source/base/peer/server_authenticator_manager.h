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

#ifndef BASE_PEER_SERVER_AUTHENTICATOR_MANAGER_H
#define BASE_PEER_SERVER_AUTHENTICATOR_MANAGER_H

#include "base/peer/server_authenticator.h"

#include <queue>

namespace base {

class ServerAuthenticatorManager final : public QObject
{
    Q_OBJECT

public:
    struct SessionInfo
    {
        SessionInfo() = default;

        SessionInfo(SessionInfo&& other) noexcept = default;
        SessionInfo& operator=(SessionInfo&& other) noexcept = default;

        SessionInfo(const SessionInfo& other) = delete;
        SessionInfo& operator=(const SessionInfo& other) = delete;

        std::unique_ptr<TcpChannel> channel;
        QVersionNumber version;
        QString os_name;
        QString computer_name;
        QString display_name;
        QString architecture;
        QString user_name;
        quint32 session_type = 0;
    };

    explicit ServerAuthenticatorManager(QObject* parent = nullptr);
    ~ServerAuthenticatorManager();

    void setUserList(std::unique_ptr<UserListBase> user_list);

    void setPrivateKey(const QByteArray& private_key);

    void setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess anonymous_access, quint32 session_types);

    // Adds a channel to the authentication queue. After success completion, a session will be
    // created (in a stopped state) and method Delegate::onNewSession will be called.
    // If authentication fails, the channel will be automatically deleted.
    void addNewChannel(std::unique_ptr<TcpChannel> channel);

    bool hasReadySessions() const;
    SessionInfo nextReadySession();

signals:
    void sig_sessionReady();

private slots:
    void onComplete();

private:
    struct Pending
    {
        std::unique_ptr<TcpChannel> channel;
        std::unique_ptr<ServerAuthenticator> authenticator;
    };

    base::local_shared_ptr<UserListBase> user_list_;
    std::vector<Pending> pending_;

    QByteArray private_key_;

    ServerAuthenticator::AnonymousAccess anonymous_access_ =
        ServerAuthenticator::AnonymousAccess::DISABLE;

    quint32 anonymous_session_types_ = 0;

    std::queue<SessionInfo> ready_sessions_;

    DISALLOW_COPY_AND_ASSIGN(ServerAuthenticatorManager);
};

} // namespace base

#endif // BASE_PEER_SERVER_AUTHENTICATOR_MANAGER_H
