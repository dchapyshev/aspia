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

#include "client/router/router_controller.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "client/config.h"
#include "client/router/router_hosts_cleaner.h"
#include "client/storage/router_store.h"
#include "client/workers/router_worker.h"
#include "common/desktop/router_error.h"
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
    connect(router_worker_, &RouterWorker::sig_reconnecting,
            this, &RouterController::onRouterReconnecting, Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_errorOccurred,
            this, &RouterController::onRouterError, Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_sessionMessageReceived,
            this, &RouterController::onRouterMessage, Qt::QueuedConnection);

    sync_worker_ = GuiApplication::findWorker<SyncWorker>();
    if (!sync_worker_)
    {
        LOG(ERROR) << "Sync worker not found";
        return;
    }

    connect(sync_worker_, &SyncWorker::sig_recordsChanged,
            this, &RouterController::onSyncRecordsChanged, Qt::QueuedConnection);
    connect(sync_worker_, &SyncWorker::sig_event,
            this, &RouterController::onSyncEvent, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
RouterController::~RouterController()
{
    if (context_)
    {
        if (context_->session)
            context_->session->rpc_.dropPending();

        finishSession();

        if (router_worker_)
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect, Qt::QueuedConnection);
    }

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
RouterStatus RouterController::status()
{
    const RouterController& controller = instance();
    if (!controller.context_)
        return RouterStatus::OFFLINE;

    const RouterContext& context = *controller.context_;
    if (context.session)
        return RouterStatus::ONLINE;

    if (context.two_factor)
        return context.two_factor->twoFactorPrompt() ? RouterStatus::TWO_FACTOR : RouterStatus::CONNECTING;

    if (!context.config->isValid())
        return RouterStatus::UNREADABLE;

    return RouterStatus::OFFLINE;
}

//--------------------------------------------------------------------------------------------------
// static
RouterSession* RouterController::session()
{
    const RouterController& controller = instance();
    return controller.context_ ? controller.context_->session.get() : nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
TwoFactorPrompt* RouterController::twoFactorPrompt()
{
    const RouterController& controller = instance();
    if (!controller.context_ || !controller.context_->two_factor)
        return nullptr;

    return controller.context_->two_factor->twoFactorPrompt();
}

//--------------------------------------------------------------------------------------------------
// static
void RouterController::startSyncRecordExchange()
{
    const RouterController& controller = instance();
    if (!controller.sync_worker_)
        return;

    QMetaObject::invokeMethod(controller.sync_worker_, &SyncWorker::onStartRecordExchange, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
// static
bool RouterController::resetSyncStorage()
{
    RouterController& controller = instance();
    if (!controller.context_)
    {
        LOG(ERROR) << "No router to reset the synchronization of";
        return false;
    }

    const RouterConfig& config = *controller.context_->config;

    if (!RouterStore().setSyncStorageResetPending(config))
    {
        LOG(ERROR) << "Unable to set the reset of the storage pending";
        return false;
    }

    if (status() != RouterStatus::ONLINE)
    {
        LOG(INFO) << "The reset of the storage is put off till the connection";
        controller.addEvent(RouterEvent::Severity::INFO,
                            tr("Data synchronization will be reset after connecting to"
                               " router %1.").arg(config.address()));
        return true;
    }

    if (controller.sync_worker_)
        QMetaObject::invokeMethod(controller.sync_worker_, &SyncWorker::onStorageResetPending, Qt::QueuedConnection);

    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterController::reload()
{
    RouterConfig config;
    const Database::FindResult found = RouterStore().find(&config);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Failed to read the router - keeping the current state";
        return;
    }

    // A router that is gone or turned off takes whatever it runs with its context.
    const bool turned_off = found == Database::FindResult::FOUND && !config.isEnabled();
    if (found == Database::FindResult::NOT_FOUND || turned_off)
    {
        if (!context_)
            return;

        finishSession();
        context_.reset();

        if (router_worker_)
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect, Qt::QueuedConnection);

        updateStatus();
        return;
    }

    // A router already running gets its fresh configuration.
    if (context_)
    {
        RouterContext& context = *context_;

        if (!config.isValid())
        {
            if (!context.session)
            {
                context.two_factor.reset();
                context.config.reset(new RouterConfig(config));

                if (router_worker_)
                {
                    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect,
                                              Qt::QueuedConnection);
                }

                updateStatus();
            }
            return;
        }

        if (context.config->hasSameAccount(config) && context.config->password() == config.password() &&
            context.config->sessionType() == config.sessionType())
        {
            const bool sync_changed = context.config->isSyncEnabled() != config.isSyncEnabled();

            // The instance is shared with whoever runs, so the edit is seen at once.
            *context.config = config;

            if (sync_changed && sync_worker_)
                QMetaObject::invokeMethod(sync_worker_, &SyncWorker::onSyncEnabledChanged, Qt::QueuedConnection);
            else if (config.isSyncStorageResetPending() && status() == RouterStatus::ONLINE && sync_worker_)
                QMetaObject::invokeMethod(sync_worker_, &SyncWorker::onStorageResetPending, Qt::QueuedConnection);
            return;
        }

        finishSession();

        context.two_factor.reset();
        context.config.reset(new RouterConfig(config));

        if (router_worker_)
        {
            addEvent(RouterEvent::Severity::INFO, tr("Connecting to router %1...").arg(config.address()));
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onReconnect, Qt::QueuedConnection);
        }
        else
        {
            startTwoFactor();
        }

        updateStatus();
        return;
    }

    context_.emplace();
    context_->config.reset(new RouterConfig(config));

    if (!config.isValid())
    {
        updateStatus();
        return;
    }

    startTwoFactor();

    if (router_worker_)
    {
        addEvent(RouterEvent::Severity::INFO, tr("Connecting to router %1...").arg(config.address()));
        QMetaObject::invokeMethod(router_worker_, &RouterWorker::onConnect, Qt::QueuedConnection);
    }

    updateStatus();
}

//--------------------------------------------------------------------------------------------------
QList<RouterEvent> RouterController::events() const
{
    return context_ ? context_->events : QList<RouterEvent>();
}

//--------------------------------------------------------------------------------------------------
void RouterController::clearEvents()
{
    if (context_)
        context_->events.clear();

    addEvent(RouterEvent::Severity::INFO, tr("Event history cleared."));
}

//--------------------------------------------------------------------------------------------------
void RouterController::addEvent(RouterEvent::Severity severity, const QString& text)
{
    const RouterEvent event{ QDateTime::currentDateTime(), severity, text };

    if (context_)
    {
        context_->events.append(event);

        while (context_->events.size() > kMaxStoredEvents)
            context_->events.removeFirst();
    }

    emit sig_event(event);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTwoFactorRequired()
{
    TwoFactorPrompt* prompt = twoFactorPrompt();
    if (!prompt)
        return;

    updateStatus();

    // The death of the question is a status transition of the router.
    connect(prompt, &QObject::destroyed, this, [this](QObject*)
    {
        updateStatus();
    });

    const QString address = context_->two_factor->config().address();

    // A blocked account has no question to answer: the router does not look at codes while the
    // block runs, so the prompt is not put on screen. The dialog of the question it replaced
    // closes with the death of the replaced prompt.
    if (prompt->blockedSeconds() > 0)
    {
        const qint64 minutes = (prompt->blockedSeconds() + 59) / 60;
        addEvent(RouterEvent::Severity::WARNING,
                 tr("Too many failed attempts. Router %1 accepts codes again"
                    " in about %2 min.").arg(address).arg(minutes));
        return;
    }

    if (prompt->codeRefused())
        addEvent(RouterEvent::Severity::WARNING, tr("The code was not accepted by router %1.").arg(address));

    addEvent(RouterEvent::Severity::WARNING, tr("Router %1 is waiting for a two-factor code.").arg(address));

    emit sig_twoFactorRequired();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTwoFactorFinished(qint64 user_id, const QVersionNumber& peer_version)
{
    if (!context_ || !context_->two_factor)
        return;

    RouterContext& context = *context_;
    context.two_factor.reset();

    bool user_stored = false;

    RouterConfig config;
    if (RouterStore().find(&config) == Database::FindResult::FOUND && config.isValid())
    {
        user_stored = config.userGuid() == context.config->userGuid();
        *context.config = config;
    }
    else
    {
        LOG(WARNING) << "Failed to re-read the router: using the in-memory copy";
    }

    startSession(user_id, peer_version, user_stored);

    addEvent(RouterEvent::Severity::INFO,
             tr("Connection to router %1 established.").arg(context.config->address()));
    updateStatus();

    // The saved credentials of hosts this router no longer knows are of no use to anyone.
    RouterHostsCleaner* cleaner = new RouterHostsCleaner(this);
    connect(cleaner, &RouterHostsCleaner::sig_finished, cleaner, &QObject::deleteLater);
    cleaner->start();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterAuthenticated(const QVersionNumber& peer_version)
{
    if (context_ && context_->two_factor)
        context_->two_factor->onStart(peer_version);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterReconnecting()
{
    if (!context_ || context_->two_factor || context_->session)
        return;

    startTwoFactor();
    updateStatus();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterError(TcpChannel::ErrorCode error_code)
{
    if (!context_)
        return;

    RouterContext& context = *context_;

    RouterEvent::Severity severity = RouterEvent::Severity::WARNING;
    if (error_code == TcpChannel::ErrorCode::CRYPTO_ERROR ||
        error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
    {
        severity = RouterEvent::Severity::CRITICAL;
    }

    addEvent(severity, tr("Network error: %1").arg(TcpChannel::errorToString(error_code)));

    // The question of a logging-in router survives the connection, so nothing else changes for
    // it. The worker keeps the reconnect cycle running by itself.
    if (context.two_factor)
    {
        context.two_factor->onConnectionLost(error_code);
        return;
    }

    if (!context.session)
        return;

    addEvent(RouterEvent::Severity::WARNING,
             tr("Disconnected from router %1.").arg(context.session->config().address()));

    // The death of the session answers everything it still owes. The worker keeps the
    // reconnect cycle running by itself, so the router goes back to logging in.
    finishSession();

    RouterConfig config;
    const Database::FindResult found = RouterStore().find(&config);
    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of the router could not be read";
        context.config.reset(new RouterConfig(config));

        if (router_worker_)
            QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect, Qt::QueuedConnection);

        updateStatus();
        return;
    }

    if (found == Database::FindResult::FOUND && config.isValid())
        *context.config = config;
    else
        LOG(WARNING) << "Failed to re-read the router: using the in-memory copy";

    addEvent(RouterEvent::Severity::INFO,
             tr("Connecting to router %1...").arg(context.config->address()));
    startTwoFactor();
    updateStatus();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRouterMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (!context_)
        return;

    if (Router2FA* two_factor = context_->two_factor.get())
    {
        two_factor->onMessageReceived(channel_id, buffer);
        return;
    }

    RouterSession* session = context_->session.get();
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
                emit sig_tempHostsChanged();
            if (notification.hosts_dirty())
                emit sig_hostsChanged();
            if (notification.relays_dirty())
                emit sig_relaysChanged();
            if (notification.clients_dirty())
                emit sig_clientsChanged();
            if (notification.users_dirty())
                emit sig_usersChanged();
            if (notification.workspaces_dirty())
                emit sig_workspacesChanged();
            if (notification.groups_dirty())
                emit sig_groupsChanged();
        }
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::onSyncEvent(SyncWorker::Event event, const QString& error_code)
{
    switch (event)
    {
        case SyncWorker::Event::STORAGE_ATTACHED:
            addEvent(RouterEvent::Severity::INFO, tr("Data synchronization is on."));
            break;

        case SyncWorker::Event::STORAGE_KEY_NOT_OPENED:
            addEvent(RouterEvent::Severity::WARNING,
                     tr("The synchronization key cannot be opened with the password of the account. Sign "
                        "in with the current password on a device that is already synchronized."));
            break;

        case SyncWorker::Event::ROUTER_ERROR:
            addEvent(RouterEvent::Severity::WARNING,
                     tr("Synchronization error: %1").arg(routerErrorText(error_code.toStdString())));
            break;

        case SyncWorker::Event::INVALID_ROUTER_ANSWER:
            addEvent(RouterEvent::Severity::WARNING,
                     tr("The router returned an invalid answer to the synchronization."));
            break;

        case SyncWorker::Event::DEVICE_ERROR:
            addEvent(RouterEvent::Severity::WARNING,
                     tr("Synchronization stopped because of an error on this device."));
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::onSyncRecordsChanged(proto::router::SyncDataType type)
{
    switch (type)
    {
        case proto::router::SYNC_DATA_TYPE_CREDENTIALS:
            emit sig_credentialsChanged();
            break;

        case proto::router::SYNC_DATA_TYPE_PROFILES:
            emit sig_profilesChanged();
            break;

        case proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS:
            emit sig_localGroupsChanged();
            break;

        case proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS:
            emit sig_localHostsChanged();
            break;

        case proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS:
            break;

        default:
            LOG(WARNING) << "Unhandled type of the synchronized records:" << type;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::startTwoFactor()
{
    CHECK(context_ && context_->config);

    Router2FA* two_factor = new Router2FA(context_->config, this);
    context_->two_factor = two_factor;

    connect(two_factor, &Router2FA::sig_twoFactorRequired, this, &RouterController::onTwoFactorRequired);
    connect(two_factor, &Router2FA::sig_twoFactorFinished, this, &RouterController::onTwoFactorFinished);
}

//--------------------------------------------------------------------------------------------------
void RouterController::startSession(qint64 user_id, const QVersionNumber& peer_version, bool user_stored)
{
    CHECK(context_ && context_->config);

    context_->session = new RouterSession(context_->config, user_id, peer_version, this);

    if (!user_stored)
    {
        LOG(ERROR) << "The record of the router does not hold the user that has signed in,"
                   << "the synchronization is not started";

        addEvent(RouterEvent::Severity::WARNING,
                 tr("Unable to save the user of router %1. Data synchronization does not work"
                    " in this connection.").arg(context_->config->address()));
    }
    else if (sync_worker_)
    {
        QMetaObject::invokeMethod(sync_worker_, &SyncWorker::onSessionStarted, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::finishSession()
{
    if (!context_ || !context_->session)
        return;

    if (sync_worker_)
        QMetaObject::invokeMethod(sync_worker_, &SyncWorker::onSessionFinished, Qt::QueuedConnection);

    context_->session.reset();
}

//--------------------------------------------------------------------------------------------------
void RouterController::updateStatus()
{
    if (!context_)
    {
        // The router just left. Its removal is the one transition announced without a context.
        emit sig_statusChanged(RouterStatus::OFFLINE);
        return;
    }

    const RouterStatus current = status();
    if (context_->last_status == current)
        return;
    context_->last_status = current;

    emit sig_statusChanged(current);
}
