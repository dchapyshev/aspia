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

#ifndef HOST_ROUTER_MANAGER_H
#define HOST_ROUTER_MANAGER_H

#include <QQueue>

#include <optional>

#include "base/build_config.h"
#include "base/scoped_qpointer.h"
#include "base/shared_pointer.h"
#include "base/time_types.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "host/database.h"
#include "proto/user.h"

namespace proto::router {
class ConnectionOffer;
} // namespace proto::router

class RelayPeerManager;

class RouterManager final : public QObject
{
    Q_OBJECT

public:
    class ConfigProvider
    {
    public:
        virtual ~ConfigProvider() = default;

        virtual bool isPortable() const = 0;

        virtual Address routerAddress() const = 0;
        virtual QByteArray routerPublicKey() const = 0;

        virtual bool oneTimePassword() const = 0;
        virtual quint32 oneTimePasswordCharacters() const = 0;
        virtual int oneTimePasswordLength() const = 0;
        virtual MilliSeconds oneTimePasswordExpire() const = 0;

        virtual QByteArray hostKey() const = 0;
        virtual bool setHostKey(const QByteArray& key) = 0;

        virtual QVector<User> userList() const = 0;
        virtual Database::PasswordProtection passwordProtectionState() const = 0;

        virtual SharedPointer<UserList> createUserList() const = 0;
    };

    explicit RouterManager(std::unique_ptr<ConfigProvider> config, QObject* parent = nullptr);
    ~RouterManager() final;

    const Address& routerAddress() const { return router_address_; }
    const QByteArray& routerPublicKey() const { return public_key_; }

    struct ReadyConnection
    {
        TcpChannel* tcp_channel = nullptr;
        QString stun_host;
        quint16 stun_port = 0;
    };

    bool hasReadyConnections() const;
    std::optional<ReadyConnection> nextReadyConnection();

public slots:
    void start();
    void onSettingsChanged();
    void onOneTimeSessionsChanged(quint32 one_time_sessions);
    void onNewOneTimePassword();
    void onUserSessionAttached();
    void onTelemetryChanged();

signals:
    void sig_routerStateChanged(const proto::user::RouterState& state);
    void sig_credentialsChanged(HostId host_id, const SecureString& one_time_password);
    void sig_clientConnected();
    void sig_removeHost();
    void sig_checkUpdates();

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onNewPeerConnected();
    void onTimer(TimePoint now);

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void routerStateChanged(proto::user::RouterState::State state);
    void hostIdRequest();
    void sendTelemetry();
    void readConnectionOffer(const proto::router::ConnectionOffer& offer);
    void renewOneTimePassword();
    User createOneTimeUser() const;

    std::unique_ptr<ConfigProvider> config_;

    ScopedQPointer<TcpChannel> tcp_channel_;
    RelayPeerManager* peer_manager_ = nullptr;
    TimePoint reconnect_time_ = TimePoint::max();

    Address router_address_ { kDefaultRouterHostTcpPort };
    QByteArray public_key_;

    TimePoint password_expire_time_ = TimePoint::max();
    SecureString one_time_password_;
    quint32 one_time_sessions_ = 0;

    SharedPointer<UserList> user_list_;

    HostId host_id_ = kInvalidHostId;
    proto::user::RouterState router_state_;

    bool telemetry_outdated_ = false;
    TimePoint next_telemetry_time_ = TimePoint::min();
    int service_start_count_ = 0;
    int successful_login_count_ = 0;
    int failed_login_count_ = 0;
    int router_connect_count_ = 0;
    TimePoint count_check_time_ = TimePoint::max();

    QQueue<ReadyConnection> channels_;

    friend class RouterManagerTestPeer;
    Q_DISABLE_COPY_MOVE(RouterManager)
};

#endif // HOST_ROUTER_MANAGER_H
