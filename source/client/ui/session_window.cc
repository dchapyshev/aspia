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

#include "client/ui/session_window.h"

#include "base/logging.h"
#include "client/client.h"
#include "client/client_proxy.h"
#include "client/status_window_proxy.h"
#include "client/ui/authorization_dialog.h"
#include "common/ui/status_dialog.h"
#include "qt_base/application.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SessionWindow::SessionWindow(std::shared_ptr<SessionState> session_state, QWidget* parent)
    : QWidget(parent),
      session_state_(std::move(session_state)),
      status_window_proxy_(
          std::make_shared<StatusWindowProxy>(base::GuiApplication::uiTaskRunner(), this))
{
    LOG(LS_INFO) << "Ctor";

    // Create a dialog to display the connection status.
    status_dialog_ = new common::StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);

    // After closing the status dialog, close the session window.
    connect(status_dialog_, &common::StatusDialog::finished, this, &SessionWindow::close);
}

//--------------------------------------------------------------------------------------------------
SessionWindow::~SessionWindow()
{
    LOG(LS_INFO) << "Dtor";
    status_window_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
bool SessionWindow::connectToHost(Config config)
{
    LOG(LS_INFO) << "Connecting to host";

    if (client_proxy_)
    {
        LOG(LS_ERROR) << "Attempt to start an already running client";
        return false;
    }

    // Set the window title.
    setClientTitle(config);

    if (config.username.isEmpty() || config.password.isEmpty())
    {
        LOG(LS_INFO) << "Empty user name or password";

        AuthorizationDialog auth_dialog(this);

        auth_dialog.setOneTimePasswordEnabled(config.router_config.has_value());
        auth_dialog.setUserName(config.username);
        auth_dialog.setPassword(config.password);

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
        {
            LOG(LS_INFO) << "Authorization rejected by user";
            return false;
        }

        config.username = auth_dialog.userName();
        config.password = auth_dialog.password();
    }

    // When connecting with a one-time password, the username must be in the following format:
    // #host_id.
    if (config.username.isEmpty())
    {
        LOG(LS_INFO) << "User name is empty. Connection by ID";
        config.username = u"#" + config.address_or_id;
    }

    session_state_ = std::make_shared<SessionState>(config);

    // Create a client instance.
    std::unique_ptr<Client> client = createClient();

    client->moveToThread(base::GuiApplication::ioThread());
    client->setStatusWindow(status_window_proxy_);
    client->setSessionState(session_state_);

    client_proxy_ = std::make_unique<ClientProxy>(
        base::GuiApplication::ioTaskRunner(), std::move(client));

    LOG(LS_INFO) << "Start client proxy";
    client_proxy_->start();
    return true;
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(LS_INFO) << "Close event";

    if (client_proxy_)
    {
        LOG(LS_INFO) << "Stopping client proxy";
        client_proxy_->stop();
        client_proxy_.reset();
    }
    else
    {
        LOG(LS_INFO) << "No client proxy";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onStarted()
{
    LOG(LS_INFO) << "Session started";
    status_dialog_->addMessageAndActivate(tr("Session started."));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onStopped()
{
    LOG(LS_INFO) << "Connection stopped";
    status_dialog_->close();
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onRouterConnecting()
{
    LOG(LS_INFO) << "Connecting to router";

    const std::optional<RouterConfig>& router = session_state_->router();
    if (router.has_value())
    {
        status_dialog_->addMessageAndActivate(
            tr("Connecting to router %1:%2...").arg(router->address).arg(router->port));
    }
    else
    {
        status_dialog_->addMessageAndActivate(tr("Connecting to router..."));
    }
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onRouterConnected()
{
    LOG(LS_INFO) << "Connection to router established";

    const std::optional<RouterConfig>& router = session_state_->router();
    if (router.has_value())
    {
        status_dialog_->addMessageAndActivate(
            tr("Connection to router %1:%2 established.").arg(router->address).arg(router->port));
    }
    else
    {
        status_dialog_->addMessageAndActivate(tr("Connection to router established."));
    }
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onHostConnecting()
{
    LOG(LS_INFO) << "Connecting to host";

    QString message;

    if (session_state_->isConnectionByHostId())
    {
        message = tr("Connecting to host %1...").arg(session_state_->hostAddress());
    }
    else
    {
        message = tr("Connecting to host %1:%2...")
                      .arg(session_state_->hostAddress())
                      .arg(session_state_->hostPort());
    }

    status_dialog_->addMessageAndActivate(message);
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onHostConnected()
{
    LOG(LS_INFO) << "Connection to host established";

    QString message;

    if (session_state_->isConnectionByHostId())
    {
        message = tr("Connection to host %1 established.").arg(session_state_->hostAddress());
    }
    else
    {
        message = tr("Connection to host %1:%2 established.")
                      .arg(session_state_->hostAddress())
                      .arg(session_state_->hostPort());
    }

    status_dialog_->addMessageAndActivate(message);
    status_dialog_->hide();
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onHostDisconnected(base::TcpChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Host disconnected";
    onErrorOccurred(netErrorToString(error_code));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onWaitForRouter()
{
    LOG(LS_INFO) << "Wait for router";
    onErrorOccurred(tr("Router is unavailable yet. Waiting to reconnect..."));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onWaitForRouterTimeout()
{
    LOG(LS_INFO) << "Wait for router timeout";
    onErrorOccurred(tr("Timeout waiting for reconnection to router."));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onWaitForHost()
{
    LOG(LS_INFO) << "Wait for host";
    onErrorOccurred(tr("Host is unavailable yet. Waiting to reconnect..."));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onWaitForHostTimeout()
{
    LOG(LS_INFO) << "Wait for host timeout";
    onErrorOccurred(tr("Timeout waiting for reconnection to host."));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onVersionMismatch()
{
    QString host_version = QString::fromStdU16String(session_state_->hostVersion().toString());
    QString client_version = QString::fromStdU16String(base::Version::kCurrentFullVersion.toString());

    onErrorOccurred(
        tr("The Host version is newer than the Client version (%1 > %2). "
           "Please update the application.")
           .arg(host_version, client_version));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onAccessDenied(base::ClientAuthenticator::ErrorCode error_code)
{
    LOG(LS_INFO) << "Authentication error";
    onErrorOccurred(authErrorToString(error_code));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onRouterError(const RouterController::Error& error)
{
    LOG(LS_INFO) << "Router error";

    switch (error.type)
    {
        case RouterController::ErrorType::NETWORK:
        {
            onErrorOccurred(tr("Network error when connecting to the router: %1")
                            .arg(netErrorToString(error.code.network)));
        }
        break;

        case RouterController::ErrorType::AUTHENTICATION:
        {
            onErrorOccurred(tr("Authentication error when connecting to the router: %1")
                            .arg(authErrorToString(error.code.authentication)));
        }
        break;

        case RouterController::ErrorType::ROUTER:
        {
            onErrorOccurred(routerErrorToString(error.code.router));
        }
        break;

        default:
            NOTREACHED();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::setClientTitle(const Config& config)
{
    QString session_name;

    switch (config.session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_name = tr("Desktop Manage");
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_name = tr("Desktop View");
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_name = tr("File Transfer");
            break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
            session_name = tr("System Information");
            break;

        case proto::SESSION_TYPE_TEXT_CHAT:
            session_name = tr("Text Chat");
            break;

        case proto::SESSION_TYPE_PORT_FORWARDING:
            session_name = tr("Port Forwarding");
            break;

        default:
            NOTREACHED();
            break;
    }

    QString computer_name = config.computer_name;
    if (computer_name.isEmpty())
    {
        if (config.router_config.has_value())
            computer_name = config.address_or_id;
        else
            computer_name = QString("%1:%2").arg(config.address_or_id).arg(config.port);
    }

    setWindowTitle(QString("%1 - %2").arg(computer_name, session_name));
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onErrorOccurred(const QString& message)
{
    hide();
    onInternalReset();

    status_dialog_->addMessageAndActivate(message);
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionWindow::netErrorToString(base::TcpChannel::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case base::TcpChannel::ErrorCode::INVALID_PROTOCOL:
            message = QT_TR_NOOP("Violation of the communication protocol.");
            break;

        case base::TcpChannel::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Cryptography error (message encryption or decryption failed).");
            break;

        case base::TcpChannel::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("An error occurred with the network (e.g., the network cable was accidentally plugged out).");
            break;

        case base::TcpChannel::ErrorCode::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case base::TcpChannel::ErrorCode::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case base::TcpChannel::ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case base::TcpChannel::ErrorCode::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case base::TcpChannel::ErrorCode::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case base::TcpChannel::ErrorCode::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        default:
        {
            if (error_code != base::TcpChannel::ErrorCode::UNKNOWN)
            {
                LOG(LS_ERROR) << "Unknown error code: " << static_cast<int>(error_code);
            }

            message = QT_TR_NOOP("An unknown error occurred.");
        }
        break;
    }

    return tr(message);
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionWindow::authErrorToString(base::ClientAuthenticator::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case base::ClientAuthenticator::ErrorCode::SUCCESS:
            message = QT_TR_NOOP("Authentication successfully completed.");
            break;

        case base::ClientAuthenticator::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("Network authentication error.");
            break;

        case base::ClientAuthenticator::ErrorCode::PROTOCOL_ERROR:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case base::ClientAuthenticator::ErrorCode::VERSION_ERROR:
            message = QT_TR_NOOP("Version of the application you are connecting to is less than "
                                 " the minimum supported version.");
            break;

        case base::ClientAuthenticator::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Wrong user name or password.");
            break;

        case base::ClientAuthenticator::ErrorCode::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    return tr(message);
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionWindow::routerErrorToString(RouterController::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case RouterController::ErrorCode::PEER_NOT_FOUND:
            message = QT_TR_NOOP("The host with the specified ID is not online.");
            break;

        case RouterController::ErrorCode::KEY_POOL_EMPTY:
            message = QT_TR_NOOP("There are no relays available or the key pool is empty.");
            break;

        case RouterController::ErrorCode::RELAY_ERROR:
            message = QT_TR_NOOP("Failed to connect to the relay server.");
            break;

        case RouterController::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Access is denied.");
            break;

        default:
            message = QT_TR_NOOP("Unknown error.");
            break;
    }

    return tr(message);
}

} // namespace client
