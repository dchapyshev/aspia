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

#include "client/ui/session_window.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "client/ui/authorization_dialog.h"
#include "common/ui/session_type.h"
#include "common/ui/status_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SessionWindow::SessionWindow(std::shared_ptr<SessionState> session_state, QWidget* parent)
    : QWidget(parent),
      session_state_(std::move(session_state))
{
    LOG(INFO) << "Ctor";

    // Create a dialog to display the connection status.
    status_dialog_ = new common::StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);

    // After closing the status dialog, close the session window.
    connect(status_dialog_, &common::StatusDialog::finished, this, &SessionWindow::close);
}

//--------------------------------------------------------------------------------------------------
SessionWindow::~SessionWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool SessionWindow::connectToHost(Config config)
{
    LOG(INFO) << "Connecting to host";

    // Set the window title.
    setClientTitle(config);

    if (config.username.isEmpty() || config.password.isEmpty())
    {
        LOG(INFO) << "Empty user name or password";

        AuthorizationDialog auth_dialog(this);

        auth_dialog.setOneTimePasswordEnabled(config.router_id > 0);
        auth_dialog.setUserName(config.username);
        auth_dialog.setPassword(config.password);

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
        {
            LOG(INFO) << "Authorization rejected by user";
            return false;
        }

        config.username = auth_dialog.userName();
        config.password = auth_dialog.password();
    }

    // When connecting with a one-time password, the username must be in the following format:
    // #host_id.
    if (config.username.isEmpty())
    {
        LOG(INFO) << "User name is empty. Connection by ID";
        config.username = u"#" + config.address_or_id;
    }

    session_state_ = std::make_shared<SessionState>(config);

    // Create a client instance.
    Client* client = createClient();

    client->moveToThread(base::GuiApplication::ioThread());
    client->setSessionState(session_state_);

    connect(client, &Client::sig_statusChanged, this, &SessionWindow::onStatusChanged, Qt::QueuedConnection);
    connect(this, &SessionWindow::sig_start, client, &Client::start, Qt::QueuedConnection);
    connect(this, &SessionWindow::sig_stop, client, &Client::deleteLater, Qt::QueuedConnection);

    LOG(INFO) << "Start client";
    emit sig_start();
    return true;
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(INFO) << "Close event";
    emit sig_stop();
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::onStatusChanged(Client::Status status, const QVariant& data)
{
    LOG(INFO) << "Client status changed:" << status;

    switch (status)
    {
        case Client::Status::STARTED:
            status_dialog_->addMessageAndActivate(tr("Session started."));
            break;

        case Client::Status::STOPPED:
            status_dialog_->close();
            break;

        case Client::Status::ROUTER_ERROR:
            onErrorOccurred(tr("Error requesting connection via router: %1.").arg(data.toString()));
            break;

        case Client::Status::NO_ROUTER:
            onErrorOccurred(tr("The specified router is unavailable."));
            break;

        case Client::Status::ROUTER_OFFLINE:
            onErrorOccurred(tr("The specified router is offline."));
            break;

        case Client::Status::HOST_CONNECTING:
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

        case Client::Status::HOST_CONNECTED:
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
        }
        break;

        case Client::Status::HOST_DISCONNECTED:
        {
            if (data.canConvert<base::TcpChannel::ErrorCode>())
            {
                base::TcpChannel::ErrorCode error_code = data.value<base::TcpChannel::ErrorCode>();
                onErrorOccurred(base::TcpChannel::errorToString(error_code));
            }
            else
            {
                LOG(ERROR) << "Unable to convert error code";
            }
        }
        break;

        case Client::Status::WAIT_FOR_HOST:
            onErrorOccurred(tr("Host is unavailable yet. Waiting to reconnect..."));
            break;

        case Client::Status::WAIT_FOR_HOST_TIMEOUT:
            onErrorOccurred(tr("Timeout waiting for reconnection to host."));
            break;

        case Client::Status::VERSION_MISMATCH:
        {
            QString host_version = session_state_->hostVersion().toString();
            QString client_version = base::kCurrentVersion.toString();

            onErrorOccurred(tr("The Host version is newer than the Client version (%1 > %2). "
                               "Please update the application.").arg(host_version, client_version));
        }
        break;

        case Client::Status::LEGACY_HOST:
            onErrorOccurred(tr("Attempting to connect in compatibility mode..."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void SessionWindow::setClientTitle(const Config& config)
{
    QString session_name = common::sessionName(config.session_type);
    QString computer_name = config.computer_name;

    if (computer_name.isEmpty())
    {
        if (config.router_id > 0)
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

} // namespace client
