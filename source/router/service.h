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

#ifndef ROUTER_SERVICE_H
#define ROUTER_SERVICE_H

#include <QList>

#include "base/service.h"
#include "base/peer/host_id.h"
#include "base/net/tcp_server.h"
#include "base/net/tcp_server_legacy.h"
#include "router/session.h"

namespace base {
class StunServer;
} // namespace base

namespace router {

class SessionHost;
class SessionRelay;

class Service final : public base::Service
{
    Q_OBJECT

public:
    explicit Service(QObject* parent = nullptr);
    ~Service() final;

    static const char kFileName[];
    static const char kName[];
    static const char kDisplayName[];
    static const char kDescription[];

    struct Credentials
    {
        qint64 session_id;
        proto::router::RelayKey key;
    };

    using Keys = QList<proto::router::RelayKey>;

    static Service* instance();

    const QList<Session*>& sessions();
    Session* session(qint64 session_id);
    bool stopSession(qint64 session_id);

    void addKey(qint64 session_id, const proto::router::RelayKey& key);
    std::optional<Credentials> takeCredentials();
    void removeKeysForRelay(qint64 session_id);
    void clearKeyPool();
    size_t keyCountForRelay(qint64 session_id) const;
    size_t keyCount() const;
    bool isKeyPoolEmpty() const;

protected:
    // base::Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onNewConnection();
    void onNewLegacyConnection();
    void onSessionFinished();
    void onHostIdAssigned(base::HostId host_id);

private:
    bool start();
    void addSession(base::TcpChannel* channel, bool is_legacy);

    base::TcpServer* tcp_server_ = nullptr;
    base::TcpServerLegacy* tcp_server_legacy_ = nullptr;
    base::StunServer* stun_server_ = nullptr;

    QMap<qint64, Keys> key_pool_;
    QList<Session*> sessions_;

    QStringList client_white_list_;
    QStringList host_white_list_;
    QStringList admin_white_list_;
    QStringList manager_white_list_;
    QStringList relay_white_list_;

    static Service* instance_;

    Q_DISABLE_COPY_MOVE(Service)
};

} // namespace router

#endif // ROUTER_SERVICE_H
