//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/client_window.h"

#include "base/logging.h"
#include "client/client.h"
#include "client/ui/authorization_dialog.h"
#include "client/ui/status_dialog.h"
#include "qt_base/application.h"

namespace client {

ClientWindow::ClientWindow(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

ClientWindow::~ClientWindow() = default;

bool ClientWindow::connectToHost(Config config)
{
    if (client_)
    {
        DLOG(LS_ERROR) << "Attempt to start an already running client";
        return false;
    }

    // Set the window title.
    setClientTitle(config);

    if (config.username.empty() || config.password.empty())
    {
        AuthorizationDialog auth_dialog(this);

        auth_dialog.setUserName(QString::fromStdU16String(config.username));
        auth_dialog.setPassword(QString::fromStdU16String(config.password));

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
            return false;

        config.username = auth_dialog.userName().toStdU16String();
        config.password = auth_dialog.password().toStdU16String();
    }

    // Create a client instance.
    client_ = createClient(qt_base::Application::taskRunner());

    // Set the window that will receive notifications.
    client_->setStatusWindow(this);

    return client_->start(config);
}

Config ClientWindow::config() const
{
    return client_->config();
}

void ClientWindow::closeEvent(QCloseEvent* event)
{
    if (client_)
        client_->stop();
}

void ClientWindow::onStarted(const std::u16string& address, uint16_t port)
{
    // Create a dialog to display the connection status.
    status_dialog_ = new StatusDialog(this);

    // After closing the status dialog, close the session window.
    connect(status_dialog_, &StatusDialog::finished, this, &ClientWindow::close);

    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);
    status_dialog_->addMessage(tr("Attempt to connect to %1:%2.").arg(address).arg(port));
}

void ClientWindow::onStopped()
{
    status_dialog_->close();
}

void ClientWindow::onConnected()
{
    status_dialog_->addMessage(tr("Connection established."));
    status_dialog_->hide();
}

void ClientWindow::onDisconnected(net::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case net::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Cryptography error (message encryption or decryption failed).");
            break;

        case net::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("An error occurred with the network (e.g., the network cable was accidentally plugged out).");
            break;

        case net::ErrorCode::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case net::ErrorCode::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case net::ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case net::ErrorCode::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case net::ErrorCode::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case net::ErrorCode::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        default:
        {
            if (error_code != net::ErrorCode::UNKNOWN)
            {
                LOG(LS_WARNING) << "Unknown error code: " << static_cast<int>(error_code);
            }

            message = QT_TR_NOOP("An unknown error occurred.");
        }
        break;
    }

    onErrorOccurred(tr(message));
}

void ClientWindow::onAccessDenied(Authenticator::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case Authenticator::ErrorCode::SUCCESS:
            message = QT_TR_NOOP("Authentication successfully completed.");
            break;

        case Authenticator::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("Network authentication error.");
            break;

        case Authenticator::ErrorCode::PROTOCOL_ERROR:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case Authenticator::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case Authenticator::ErrorCode::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    onErrorOccurred(tr(message));
}

void ClientWindow::setClientTitle(const Config& config)
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

        default:
            NOTREACHED();
            break;
    }

    QString computer_name = QString::fromStdU16String(config.computer_name);
    if (computer_name.isEmpty())
        computer_name = QString::fromStdU16String(config.address);

    setWindowTitle(QString("%1 - %2").arg(computer_name).arg(session_name));
}

void ClientWindow::onErrorOccurred(const QString& message)
{
    hide();

    for (const auto& object : children())
    {
        QWidget* widget = dynamic_cast<QWidget*>(object);
        if (widget)
            widget->hide();
    }

    status_dialog_->addMessage(message);
}

} // namespace client
