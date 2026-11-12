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

#ifndef CLIENT_ROUTER_ROUTER_CONTROLLER_H
#define CLIENT_ROUTER_ROUTER_CONTROLLER_H

#include <QObject>

#include <optional>

#include "base/shared_pointer.h"
#include "client/router/router_2fa.h"
#include "client/router/router_session.h"

class RouterController final : public QObject
{
    Q_OBJECT

public:
    // Only one lives at a time, and the static lookups below answer through it.
    explicit RouterController(QObject* parent = nullptr);
    ~RouterController() final;

    static RouterController& instance();
    static RouterStatus status();

    // The working session of the router, nullptr unless the router is ONLINE. The object dies
    // with the connection, so it is looked up at use and never kept.
    static RouterSession* session();

    // The two-factor question of the router, nullptr while nothing is asked. Answered and
    // dismissed through the object itself; who shows it and when is the business of the GUI.
    static TwoFactorPrompt* twoFactorPrompt();

    // Syncs the login and the session with the stored router. A new router with usable
    // credentials starts logging in, a router that stays receives its fresh config, and
    // everything a router that is gone was running is destroyed.
    void reload();

    static constexpr int kMaxStoredEvents = 100;

    QList<RouterEvent> events() const;
    void clearEvents();
    void addEvent(RouterEvent::Severity severity, const QString& text);

signals:
    void sig_statusChanged(RouterStatus status);
    void sig_event(const RouterEvent& event);
    void sig_twoFactorRequired();
    void sig_tempHostsChanged();
    void sig_hostsChanged();
    void sig_relaysChanged();
    void sig_clientsChanged();
    void sig_usersChanged();
    void sig_workspacesChanged();
    void sig_groupsChanged();

private slots:
    void onTwoFactorRequired();
    void onTwoFactorFinished(qint64 user_id, const QVersionNumber& peer_version);
    void onRouterAuthenticated(const QVersionNumber& peer_version);
    void onRouterReconnecting();
    void onRouterError(TcpChannel::ErrorCode error_code);
    void onRouterMessage(quint8 channel_id, const QByteArray& buffer);

private:
    struct RouterContext
    {
        SharedPointer<RouterConfig> config;
        ScopedQPointer<Router2FA> two_factor;
        ScopedQPointer<RouterSession> session;
        RouterStatus last_status = RouterStatus::OFFLINE;
        QList<RouterEvent> events;
    };

    void startTwoFactor();
    void updateStatus();

    std::optional<RouterContext> context_;
    QPointer<RouterWorker> router_worker_;

    Q_DISABLE_COPY_MOVE(RouterController)
};

#endif // CLIENT_ROUTER_ROUTER_CONTROLLER_H
