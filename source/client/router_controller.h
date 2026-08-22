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

#ifndef CLIENT_ROUTER_CONTROLLER_H
#define CLIENT_ROUTER_CONTROLLER_H

#include <QObject>

#include <unordered_map>

#include "base/shared_pointer.h"
#include "client/router_2fa.h"
#include "client/router_session.h"

// The single owner of the router records at runtime. It runs their logins (Router2FA),
// births the working sessions (RouterSession) from the completed ones and destroys them with their
// connections. The signals of the short-lived sessions are relayed under its own stable ones,
// and the connection journal is written here for both platforms.
class RouterController final : public QObject
{
    Q_OBJECT

public:
    explicit RouterController(QObject* parent = nullptr);
    ~RouterController() final;

    static RouterController& instance();
    static RouterStatus status(qint64 router_id);

    // The working session of the record, nullptr unless the record is ONLINE. The object dies
    // with the connection, so it is looked up at use and never kept.
    static RouterSession* session(qint64 router_id);

    // The two-factor question of the record, nullptr while nothing is asked. Answered and
    // dismissed through the object itself; who shows it and when is the business of the GUI.
    static TwoFactorPrompt* twoFactorPrompt(qint64 router_id);

    // Syncs the logins and the sessions with the stored router records. A new record with
    // usable credentials starts logging in, a record that stays receives its fresh config, and
    // everything a record that is gone was running is destroyed.
    void reload();

    QList<RouterEvent> events(qint64 router_id) const;
    void clearEvents(qint64 router_id);
    void addEvent(qint64 router_id, RouterEvent::Severity severity, const QString& text);

signals:
    void sig_created(qint64 router_id);
    void sig_statusChanged(qint64 router_id, RouterStatus status);
    void sig_event(qint64 router_id, const RouterEvent& event);
    void sig_twoFactorRequired(qint64 router_id);
    void sig_tempHostsChanged(qint64 router_id);
    void sig_hostsChanged(qint64 router_id);
    void sig_relaysChanged(qint64 router_id);
    void sig_clientsChanged(qint64 router_id);
    void sig_usersChanged(qint64 router_id);
    void sig_workspacesChanged(qint64 router_id);
    void sig_groupsChanged(qint64 router_id);

private slots:
    void onTwoFactorRequired(qint64 router_id);
    void onTwoFactorUndelivered(qint64 router_id);
    void onTwoFactorFinished(qint64 router_id, qint64 user_id, const QVersionNumber& peer_version);
    void onRouterAuthenticated(qint64 router_id, const QVersionNumber& peer_version);
    void onRouterError(qint64 router_id, TcpChannel::ErrorCode error_code);
    void onRouterMessage(qint64 router_id, quint8 channel_id, const QByteArray& buffer);

private:
    struct RouterContext
    {
        SharedPointer<RouterConfig> config;
        ScopedQPointer<Router2FA> two_factor;
        ScopedQPointer<RouterSession> session;
        RouterStatus last_status = RouterStatus::OFFLINE;
        QList<RouterEvent> events;
    };

    void startTwoFactor(qint64 router_id, const RouterConfig& config);
    void updateStatus(qint64 router_id);

    std::unordered_map<qint64, RouterContext> contexts_;
    QPointer<RouterWorker> router_worker_;

    Q_DISABLE_COPY_MOVE(RouterController)
};

#endif // CLIENT_ROUTER_CONTROLLER_H
