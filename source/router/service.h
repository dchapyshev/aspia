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
#include "base/net/tcp_server.h"
#include "base/net/tcp_server_legacy.h"
#include "router/session.h"

namespace router {

class DatabaseFactory;
class KeyFactory;
class SessionHost;
class SessionManager;
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

    static Service* instance();

    QList<Session*> sessions();
    Session* session(qint64 session_id);
    bool stopSession(qint64 session_id);

protected:
    // base::Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onPoolKeyUsed(qint64 session_id, quint32 key_id);
    void onNewConnection();
    void onNewLegacyConnection();
    void onSessionFinished();

private:
    bool start();
    void addSession(base::TcpChannel* channel);

    base::SharedPointer<DatabaseFactory> database_factory_;
    base::TcpServer* tcp_server_ = nullptr;
    base::TcpServerLegacy* tcp_server_legacy_ = nullptr;
    KeyFactory* key_factory_ = nullptr;

    QList<Session*> sessions_;

    QStringList client_white_list_;
    QStringList host_white_list_;
    QStringList admin_white_list_;
    QStringList relay_white_list_;

    static Service* instance_;

    Q_DISABLE_COPY_MOVE(Service)
};

} // namespace router

#endif // ROUTER_SERVICE_H
