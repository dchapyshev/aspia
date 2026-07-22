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

#include "client/desktop/client_window.h"

#include <QAction>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QMoveEvent>
#include <QTimer>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/threading/worker.h"
#include "client/router.h"
#include "client/session_keeper.h"
#include "client/desktop/authorization_dialog.h"
#include "client/workers/network_worker.h"
#include "common/desktop/session_type.h"
#include "common/desktop/status_dialog.h"
#include "proto/peer.h"
#include "proto/router_client.h"

namespace {

constexpr int kDragPollIntervalMs = 50;
const Seconds kReconnectRetryDelay { 5 };
const Minutes kReconnectBudget { 5 };

} // namespace

//--------------------------------------------------------------------------------------------------
ClientWindow::ClientWindow(proto::peer::SessionType session_type, QWidget* parent)
    : QWidget(parent),
      session_type_(session_type),
      drag_poll_timer_(new QTimer(this)),
      reconnect_timeout_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    // Create a dialog to display the connection status.
    status_dialog_ = new StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);

    // After closing the status dialog, close the session window.
    connect(status_dialog_, &StatusDialog::finished, this, &ClientWindow::close);

    drag_poll_timer_->setInterval(kDragPollIntervalMs);
    connect(drag_poll_timer_, &QTimer::timeout, this, &ClientWindow::onDragPoll);

    reconnect_timeout_timer_->setSingleShot(true);
    connect(reconnect_timeout_timer_, &QTimer::timeout, this, [this]()
    {
        session_state_->setReconnecting(false);
        session_state_->setAutoReconnect(false);
        onErrorOccurred(tr("Timeout waiting for reconnection to host."));
    });

    createSessionConnectActions();
}

//--------------------------------------------------------------------------------------------------
ClientWindow::~ClientWindow()
{
    LOG(INFO) << "Dtor";

    if (session_keeper_)
        session_keeper_->release();
}

//--------------------------------------------------------------------------------------------------
bool ClientWindow::connectToHost(HostConfig host, const QString& display_name)
{
    LOG(INFO) << "Connecting to host";

    // Set the window title.
    setClientTitle(host, session_type_);

    if (host.username().isEmpty() || host.password().isEmpty())
    {
        LOG(INFO) << "Empty user name or password";

        AuthorizationDialog auth_dialog(this);

        auth_dialog.setOneTimePasswordEnabled(host.routerId() > 0);
        auth_dialog.setUserName(host.username());
        auth_dialog.setPassword(host.password());

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
        {
            LOG(INFO) << "Authorization rejected by user";
            return false;
        }

        host.setUsername(auth_dialog.userName());
        host.setPassword(auth_dialog.password());
    }

    // When connecting with a one-time password, the username must be in the following format:
    // #host_id.
    if (host.username().isEmpty())
    {
        LOG(INFO) << "User name is empty. Connection by ID";
        host.setUsername(u"#" + host.address());
    }

    session_state_ = std::make_shared<SessionState>(host, session_type_, display_name);

    LOG(INFO) << "Start client";
    if (session_state_->isConnectionByHostId())
    {
        // Relay path: fetch the ConnectionOffer first; the session starts once we have it.
        fetchConnectionOffer();
    }
    else
    {
        startNewSession();
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::setTabbedMode(bool /* tabbed */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> ClientWindow::tabActionGroups() const
{
    return {{ Tab::ActionRole::ACTION, session_connect_actions_ }};
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::createSessionConnectActions()
{
    static const proto::peer::SessionType kOrder[] = {
        proto::peer::SESSION_TYPE_DESKTOP,
        proto::peer::SESSION_TYPE_TERMINAL,
        proto::peer::SESSION_TYPE_FILE_TRANSFER,
        proto::peer::SESSION_TYPE_SYSTEM_INFO,
        proto::peer::SESSION_TYPE_CHAT
    };

    for (proto::peer::SessionType type : kOrder)
    {
        if (type == session_type_)
            continue;

        QAction* action = new QAction(sessionIcon(type), sessionName(type), this);
        connect(action, &QAction::triggered, this, [this, type]()
        {
            emit sig_connectRequested(sessionState()->host(), type);
        });
        session_connect_actions_.append(action);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::applySettings()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QByteArray ClientWindow::saveState() const
{
    return {};
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::restoreState(const QByteArray& /* state */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::addWorker(std::unique_ptr<Worker> worker)
{
    worker_manager_->add(std::move(worker));
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::sendMessage(quint8 channel_id, const QByteArray& buffer)
{
    emit sig_sendMessage(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
NetworkWorker* ClientWindow::networkWorker() const
{
    return network_worker_;
}

//--------------------------------------------------------------------------------------------------
bool ClientWindow::isLegacy() const
{
    return is_legacy_mode_;
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(INFO) << "Close event";
    drag_poll_timer_->stop();
    worker_manager_.reset();
    emit sig_stop();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);

    // Drag detection only makes sense when this widget is acting as a top-level window (i.e. the
    // session is currently detached from the tab bar). When embedded as a child, moveEvent fires
    // from layout repositioning and must be ignored.
    if (window() != this)
        return;

    if (drag_poll_timer_->isActive())
        return;

    if (!(QGuiApplication::mouseButtons() & Qt::LeftButton))
        return;

    drag_poll_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    LOG(INFO) << "Client status changed:" << status;

    switch (status)
    {
        case NetworkWorker::Status::STARTED:
            status_dialog_->addMessageAndActivate(tr("Session started."));
            break;

        case NetworkWorker::Status::RELAY_ERROR:
            onErrorOccurred(data.toString());
            break;

        case NetworkWorker::Status::HOST_CONNECTING:
        {
            if (session_state_->isConnectionByHostId())
            {
                status_dialog_->addMessageAndActivate(
                    tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            }
            else
            {
                status_dialog_->addMessageAndActivate(tr("Connecting to host %1:%2...")
                    .arg(session_state_->hostAddress()).arg(session_state_->hostPort()));
            }
        }
        break;

        case NetworkWorker::Status::HOST_CONNECTED:
        {
            if (session_state_->isConnectionByHostId())
            {
                status_dialog_->addMessageAndActivate(
                    tr("Connection to host %1 established.").arg(session_state_->hostAddress()));
            }
            else
            {
                status_dialog_->addMessageAndActivate(tr("Connection to host %1:%2 established.")
                    .arg(session_state_->hostAddress()).arg(session_state_->hostPort()));
            }

            status_dialog_->setVisible(false);
            reconnect_timeout_timer_->stop();
            emit sig_connected();
        }
        break;

        case NetworkWorker::Status::HOST_DISCONNECTED:
        {
            if (data.canConvert<TcpChannel::ErrorCode>())
            {
                TcpChannel::ErrorCode error_code = data.value<TcpChannel::ErrorCode>();
                onErrorOccurred(TcpChannel::errorToString(error_code));
            }
            else
            {
                LOG(ERROR) << "Unable to convert error code";
            }
        }
        break;

        case NetworkWorker::Status::WAIT_FOR_HOST:
            onErrorOccurred(tr("Host is unavailable yet. Waiting to reconnect..."));
            if (!session_state_->isAutoReconnect())
                break;
            // First disconnect in this auto-reconnect cycle: arm the overall timeout. Subsequent
            // WAIT_FOR_HOST signals during the same cycle keep the deadline rolling.
            if (!reconnect_timeout_timer_->isActive())
                reconnect_timeout_timer_->start(kReconnectBudget);
            // Relay path fetches a fresh ConnectionOffer (which then starts the session).
            // Direct path doesn't need an offer - just rebuild the session after the same delay.
            if (session_state_->isConnectionByHostId())
                QTimer::singleShot(kReconnectRetryDelay, this, &ClientWindow::fetchConnectionOffer);
            else
                QTimer::singleShot(kReconnectRetryDelay, this, &ClientWindow::startNewSession);
            break;

        case NetworkWorker::Status::VERSION_MISMATCH:
        {
            QString host_version = session_state_->hostVersion().toString();
            QString client_version = kCurrentVersion.toString();

            onErrorOccurred(tr("The Host version is newer than the Client version (%1 > %2). "
                               "Please update the application.").arg(host_version, client_version));
        }
        break;

        case NetworkWorker::Status::LEGACY_HOST:
            onErrorOccurred(tr("Attempting to connect in compatibility mode..."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onDragPoll()
{
    if (QGuiApplication::mouseButtons() & Qt::LeftButton)
    {
        emit sig_dragMove(QCursor::pos());
        return;
    }

    drag_poll_timer_->stop();
    emit sig_dragFinished(QCursor::pos());
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::setClientTitle(const HostConfig& host, proto::peer::SessionType session_type)
{
    QString session_name = sessionName(session_type);
    QString computer_name = host.name().isEmpty() ? host.address() : host.name();

    setWindowTitle(QString("%1 - %2").arg(computer_name, session_name));
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onErrorOccurred(const QString& message)
{
    hide();
    onInternalReset();

    status_dialog_->addMessageAndActivate(message);
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::fetchConnectionOffer()
{
    // If auto-reconnect was disabled (e.g. Client's reconnect timeout elapsed), stop trying.
    if (session_state_->isReconnecting() && !session_state_->isAutoReconnect())
        return;

    Router* router = Router::instance(session_state_->routerId());
    if (!router)
    {
        onErrorOccurred(tr("The specified router is unavailable."));
        return;
    }

    if (router->status() != Router::Status::ONLINE)
    {
        onErrorOccurred(tr("The specified router is offline."));
        return;
    }

    session_state_->setRouterVersion(router->version());

    if (!session_state_->isReconnecting())
        status_dialog_->addMessageAndActivate(tr("Requesting connection to the host..."));

    router->requestConnection(session_state_->hostId(), this,
        [this](const proto::router::ConnectionOffer& offer)
    {
        if (offer.error_code() == proto::router::ConnectionOffer::SUCCESS)
        {
            if (!session_state_->isReconnecting())
                status_dialog_->addMessageAndActivate(tr("Connection offer received."));

            session_state_->setConnectionOffer(offer);
            startNewSession();
            return;
        }

        if (offer.error_code() == proto::router::ConnectionOffer::PEER_NOT_FOUND &&
            session_state_->isReconnecting())
        {
            // Host is offline - wait a few seconds and try again. Client's internal
            // timeout_timer will eventually flip isAutoReconnect to false;
            // fetchConnectionOffer() guards on that.
            QTimer::singleShot(kReconnectRetryDelay, this, &ClientWindow::fetchConnectionOffer);
            return;
        }

        QString error;
        switch (offer.error_code())
        {
            case proto::router::ConnectionOffer::PEER_NOT_FOUND:
                error = tr("The host with the specified ID is not online");
                break;
            case proto::router::ConnectionOffer::ACCESS_DENIED:
                error = tr("Access is denied");
                break;
            case proto::router::ConnectionOffer::KEY_POOL_EMPTY:
                error = tr("There are no relays available or the key pool is empty");
                break;
            default:
                error = tr("Unknown error");
                break;
        }
        onErrorOccurred(tr("Error requesting connection via router: %1.").arg(error));
    });
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::startNewSession()
{
    if (!session_keeper_)
        session_keeper_ = SessionKeeper::create(this);

    worker_manager_.reset();
    worker_manager_ = std::make_unique<WorkerManager>();

    std::unique_ptr<NetworkWorker> network_worker = std::make_unique<NetworkWorker>();
    network_worker_ = network_worker.get();
    worker_manager_->add(std::move(network_worker));

    onRegisterWorkers();

    connect(this, &ClientWindow::sig_startConnection, network_worker_, &NetworkWorker::onStartConnection,
            Qt::QueuedConnection);
    connect(this, &ClientWindow::sig_sessionReady, network_worker_, &NetworkWorker::onSessionReady,
            Qt::QueuedConnection);
    connect(this, &ClientWindow::sig_sendMessage, network_worker_, &NetworkWorker::onSendMessage,
            Qt::QueuedConnection);

    connect(network_worker_, &NetworkWorker::sig_statusChanged, this, &ClientWindow::onNetworkStatusChanged,
            Qt::QueuedConnection);

    worker_manager_->start();

    emit sig_startConnection(session_state_);
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    if (status == NetworkWorker::Status::LEGACY_HOST)
    {
        is_legacy_mode_ = true;
    }
    else if (status == NetworkWorker::Status::HOST_DISCONNECTED)
    {
        if (session_keeper_)
            session_keeper_->release();
    }

    onStatusChanged(status, data);

    if (status == NetworkWorker::Status::HOST_CONNECTED)
        onNetworkConnected();
}

//--------------------------------------------------------------------------------------------------
void ClientWindow::onNetworkConnected()
{
    if (session_keeper_)
        session_keeper_->acquire();

    // Set up the session and show the window.
    onSessionStarted();

    // Now the session will receive incoming messages.
    emit sig_sessionReady();
}
