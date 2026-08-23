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

#include "client/router_controller.h"

#include <QSet>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router_hosts_cleaner.h"
#include "client/workers/router_worker.h"
#include "proto/router_constants.h"

namespace {

RouterController* g_instance = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterController::RouterController(QObject* parent)
    : QObject(parent)
{
    CHECK(!g_instance);
    g_instance = this;

    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
    {
        LOG(ERROR) << "Router worker not found";
        return;
    }

    connect(router_worker_, &RouterWorker::sig_authenticated,
            this, &RouterController::onRouterAuthenticated, Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_errorOccurred,
            this, &RouterController::onRouterError, Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_messageReceived,
            this, &RouterController::onRouterMessage, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
RouterController::~RouterController()
{
    g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
RouterController& RouterController::instance()
{
    CHECK(g_instance);
    return *g_instance;
}

//--------------------------------------------------------------------------------------------------
// static
RouterStatus RouterController::status(qint64 router_id)
{
    const RouterController& controller = instance();

    auto it = controller.contexts_.find(router_id);
    if (it == controller.contexts_.end())
        return RouterStatus::OFFLINE;

    const RouterContext& context = it->second;
    if (context.session)
        return RouterStatus::ONLINE;

    if (context.two_factor)
        return context.two_factor->twoFactorPrompt() ? RouterStatus::TWO_FACTOR : RouterStatus::CONNECTING;

    return RouterStatus::OFFLINE;
}

//--------------------------------------------------------------------------------------------------
// static
RouterSession* RouterController::session(qint64 router_id)
{
    const RouterController& controller = instance();

    auto it = controller.contexts_.find(router_id);
    return it != controller.contexts_.end() ? it->second.session.get() : nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
TwoFactorPrompt* RouterController::twoFactorPrompt(qint64 router_id)
{
    const RouterController& controller = instance();

    auto it = controller.contexts_.find(router_id);
    if (it == controller.contexts_.end() || !it->second.two_factor)
        return nullptr;

    return it->second.two_factor->twoFactorPrompt();
}

//--------------------------------------------------------------------------------------------------
void RouterController::reload()
{
    QList<RouterConfig> configs;
    if (!Database::instance().routerList(&configs))
    {
        LOG(ERROR) << "Failed to read the router list - keeping the current state";
        return;
    }

    // A record whose sealed column did not open comes back with empty credentials and is not served.
    QSet<qint64> present;
    present.reserve(configs.size());
    for (const RouterConfig& config : configs)
    {
        if (config.isValid())
            present.insert(config.routerId());
    }

    // The dead records go first. Whatever they run leaves with the context.
    QList<qint64> known;
    for (const auto& entry : contexts_)
        known.append(entry.first);
    for (qint64 router_id : std::as_const(known))
    {
        if (present.contains(router_id))
            continue;

        contexts_.erase(router_id);

        if (router_worker_)
        {
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect,
                                      Qt::QueuedConnection, router_id);
        }

        updateStatus(router_id);
    }

    // Connect the new records, refresh the configuration of the ones already running.
    for (const RouterConfig& config : configs)
    {
        if (!config.isValid())
            continue;

        const qint64 router_id = config.routerId();

        auto it = contexts_.find(router_id);
        if (it != contexts_.end() && (it->second.two_factor || it->second.session))
        {
            RouterContext& context = it->second;

            // An edit that renames the record keeps what is running; one that points it at
            // another account replaces it whole, because everything the running login or
            // session holds (the question on screen included) belongs to the account that is
            // gone.
            if (context.config->hasSameParams(config))
            {
                // The instance is shared with whoever runs, so the edit is seen at once.
                *context.config = config;
                continue;
            }

            context.two_factor.reset();
            context.session.reset();

            startTwoFactor(router_id, config);

            if (router_worker_)
            {
                addEvent(router_id, RouterEvent::Severity::INFO,
                         tr("Connecting to router %1...").arg(config.address()));
                QMetaObject::invokeMethod(router_worker_, &RouterWorker::onReconnect,
                                          Qt::QueuedConnection, router_id);
            }

            updateStatus(router_id);
            continue;
        }

        startTwoFactor(router_id, config);

        if (router_worker_)
        {
            addEvent(router_id, RouterEvent::Severity::INFO,
                     tr("Connecting to router %1...").arg(config.address()));
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onConnect,
                                      Qt::QueuedConnection, router_id);
        }

        updateStatus(router_id);
    }
}

//--------------------------------------------------------------------------------------------------
QList<RouterEvent> RouterController::events(qint64 router_id) const
{
    auto it = contexts_.find(router_id);
    return it != contexts_.end() ? it->second.events : QList<RouterEvent>();
}

//--------------------------------------------------------------------------------------------------
void RouterController::clearEvents(qint64 router_id)
{
    auto it = contexts_.find(router_id);
    if (it != contexts_.end())
        it->second.events.clear();

    addEvent(router_id, RouterEvent::Severity::INFO, tr("Event history cleared."));
}

//--------------------------------------------------------------------------------------------------
void RouterController::addEvent(qint64 router_id, RouterEvent::Severity severity,
                               const QString& text)
{
    const RouterEvent event{ QDateTime::currentDateTime(), severity, text };

    auto it = contexts_.find(router_id);
    if (it != contexts_.end())
    {
        it->second.events.append(event);

        while (it->second.events.size() > kMaxStoredEvents)
            it->second.events.removeFirst();
    }

    emit sig_event(router_id, event);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTwoFactorRequired(qint64 router_id)
{
    TwoFactorPrompt* prompt = twoFactorPrompt(router_id);
    if (!prompt)
        return;

    updateStatus(router_id);

    // The death of the question is a status transition of the record.
    connect(prompt, &QObject::destroyed, this, [this, router_id](QObject*)
    {
        updateStatus(router_id);
    });

    const QString address = contexts_.find(router_id)->second.two_factor->config().address();

    // A blocked account has no question to answer: the router does not look at codes while the
    // block runs, so the prompt is not put on screen. The dialog of the question it replaced
    // closes with the death of the replaced prompt.
    if (prompt->blockedSeconds() > 0)
    {
        const qint64 minutes = (prompt->blockedSeconds() + 59) / 60;
        addEvent(router_id, RouterEvent::Severity::WARNING,
                 tr("Too many failed attempts. Router %1 accepts codes again"
                    " in about %2 min.").arg(address).arg(minutes));
        return;
    }

    if (prompt->codeRefused())
    {
        addEvent(router_id, RouterEvent::Severity::WARNING,
                 tr("The code was not accepted by router %1.").arg(address));
    }
    addEvent(router_id, RouterEvent::Severity::WARNING,
             tr("Router %1 is waiting for a two-factor code.").arg(address));

    emit sig_twoFactorRequired(router_id);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTwoFactorFinished(
    qint64 router_id, qint64 user_id, const QVersionNumber& peer_version)
{
    auto it = contexts_.find(router_id);
    if (it == contexts_.end() || !it->second.two_factor)
        return;

    RouterContext& context = it->second;
    context.two_factor.reset();

    const std::optional<RouterConfig> config = Database::instance().findRouter(router_id);
    if (config.has_value() && config->isValid())
        *context.config = *config;
    else
        LOG(WARNING) << "Failed to re-read record (router_id:" << router_id << ") - using the in-memory copy";

    context.session = new RouterSession(context.config, user_id, peer_version, this);

    addEvent(router_id, RouterEvent::Severity::INFO,
             tr("Connection to router %1 established.").arg(context.config->address()));
    updateStatus(router_id);
    emit sig_created(router_id);

    // The saved credentials of hosts this router no longer knows are of no use to anyone.
    RouterHostsCleaner* cleaner = new RouterHostsCleaner(router_id, this);
    connect(cleaner, &RouterHostsCleaner::sig_finished, cleaner, &QObject::deleteLater);
    cleaner->start();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterAuthenticated(qint64 router_id, const QVersionNumber& peer_version)
{
    auto it = contexts_.find(router_id);
    if (it != contexts_.end() && it->second.two_factor)
        it->second.two_factor->onStart(peer_version);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterError(qint64 router_id, TcpChannel::ErrorCode error_code)
{
    auto it = contexts_.find(router_id);
    if (it == contexts_.end())
        return;

    RouterContext& context = it->second;

    RouterEvent::Severity severity = RouterEvent::Severity::WARNING;
    if (error_code == TcpChannel::ErrorCode::CRYPTO_ERROR ||
        error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
    {
        severity = RouterEvent::Severity::CRITICAL;
    }

    addEvent(router_id, severity,
             tr("Network error: %1").arg(TcpChannel::errorToString(error_code)));

    // The question of a logging-in record survives the connection, so nothing else changes for
    // it. The worker keeps the reconnect cycle running by itself.
    if (context.two_factor)
    {
        context.two_factor->onConnectionLost(error_code);
        return;
    }

    if (!context.session)
        return;

    addEvent(router_id, RouterEvent::Severity::WARNING,
             tr("Disconnected from router %1.").arg(context.session->config().address()));

    // The death of the session answers everything it still owes. The worker keeps the
    // reconnect cycle running by itself, so the record goes back to logging in.
    context.session.reset();

    const std::optional<RouterConfig> config = Database::instance().findRouter(router_id);
    if (config.has_value() && config->isValid())
        *context.config = *config;
    else
        LOG(WARNING) << "Failed to re-read record (router_id:" << router_id << ") - using the in-memory copy";

    addEvent(router_id, RouterEvent::Severity::INFO,
             tr("Connecting to router %1...").arg(context.config->address()));
    startTwoFactor(router_id, *context.config);
    updateStatus(router_id);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterMessage(qint64 router_id, quint8 channel_id, const QByteArray& buffer)
{
    auto it = contexts_.find(router_id);
    if (it == contexts_.end())
        return;

    if (Router2FA* two_factor = it->second.two_factor.get())
    {
        two_factor->onMessageReceived(channel_id, buffer);
        return;
    }

    RouterSession* session = it->second.session.get();
    if (!session)
        return;

    if (channel_id == proto::router::CHANNEL_ID_ADMIN)
    {
        proto::router::RouterToAdmin message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse admin message";
            return;
        }

        session->onMessageReceived(message);
    }
    else if (channel_id == proto::router::CHANNEL_ID_MANAGER)
    {
        proto::router::RouterToManager message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse manager message";
            return;
        }

        session->onMessageReceived(message);
    }
    else if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        proto::router::RouterToClient message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse client message";
            return;
        }

        session->onMessageReceived(message);

        // The signals go after the handoff, so a subscriber that refetches on one of them is
        // not served the lists the notification just outdated.
        if (message.has_notification())
        {
            const proto::router::Notification& notification = message.notification();
            if (notification.temp_hosts_dirty())
                emit sig_tempHostsChanged(router_id);
            if (notification.hosts_dirty())
                emit sig_hostsChanged(router_id);
            if (notification.relays_dirty())
                emit sig_relaysChanged(router_id);
            if (notification.clients_dirty())
                emit sig_clientsChanged(router_id);
            if (notification.users_dirty())
                emit sig_usersChanged(router_id);
            if (notification.workspaces_dirty())
                emit sig_workspacesChanged(router_id);
            if (notification.groups_dirty())
                emit sig_groupsChanged(router_id);
        }
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::startTwoFactor(qint64 router_id, const RouterConfig& config)
{
    RouterContext& context = contexts_[router_id];
    context.config.reset(new RouterConfig(config));

    Router2FA* two_factor = new Router2FA(context.config, this);
    context.two_factor = two_factor;

    connect(two_factor, &Router2FA::sig_twoFactorRequired, this, &RouterController::onTwoFactorRequired);
    connect(two_factor, &Router2FA::sig_twoFactorFinished, this, &RouterController::onTwoFactorFinished);
}

//--------------------------------------------------------------------------------------------------
void RouterController::updateStatus(qint64 router_id)
{
    auto it = contexts_.find(router_id);
    if (it == contexts_.end())
    {
        // The record just left. Its removal is the one transition announced without a context.
        emit sig_statusChanged(router_id, RouterStatus::OFFLINE);
        return;
    }

    const RouterStatus current = status(router_id);
    if (it->second.last_status == current)
        return;
    it->second.last_status = current;

    emit sig_statusChanged(router_id, current);
}
