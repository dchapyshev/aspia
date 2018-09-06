//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client.h"

#include "client/ui/status_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "client/client_session_file_transfer.h"
#include "client/client_session_system_info.h"

namespace aspia {

Client::Client(const ConnectData& connect_data, QObject* parent)
    : QObject(parent),
      connect_data_(connect_data)
{
    // Create a network channel.
    network_channel_ = new NetworkChannelClient(this);

    // Create a status dialog. It displays all information about the progress of the connection
    // and errors.
    status_dialog_ = new StatusDialog();

    connect(network_channel_, &NetworkChannelClient::connected, this, &Client::onChannelConnected);
    connect(network_channel_, &NetworkChannelClient::errorOccurred, this, &Client::onChannelError);
    connect(network_channel_, &NetworkChannelClient::disconnected, this, &Client::onChannelDisconnected);

    connect(status_dialog_, &StatusDialog::finished, [this](int /* result */)
    {
        // When the status dialog is finished, we stop the connection.
        network_channel_->stop();

        // Delete the dialog after the finish.
        status_dialog_->deleteLater();

        // When the status dialog is finished, we call the client's termination.
        emit finished(this);
    });
}

void Client::start()
{
    status_dialog_->show();
    status_dialog_->activateWindow();

    status_dialog_->addStatus(tr("Attempt to connect to %1:%2.")
                              .arg(connect_data_.address())
                              .arg(connect_data_.port()));

    network_channel_->connectToHost(connect_data_.address(), connect_data_.port(),
                                    connect_data_.userName(), connect_data_.password(),
                                    connect_data_.sessionType());
}

void Client::onChannelConnected()
{
    status_dialog_->addStatus(tr("Connection established."));

    switch (connect_data_.sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_ = new ClientSessionDesktopManage(&connect_data_, this);
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_ = new ClientSessionDesktopView(&connect_data_, this);
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_ = new ClientSessionFileTransfer(&connect_data_, this);
            break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
            session_ = new ClientSessionSystemInfo(&connect_data_, this);
            break;

        default:
            status_dialog_->addStatus(tr("Unsupported session type."));
            return;
    }

    // Messages received from the network are sent to the session.
    connect(network_channel_, &NetworkChannel::messageReceived, session_, &ClientSession::messageReceived);
    connect(session_, &ClientSession::sendMessage, network_channel_, &NetworkChannel::send);
    connect(network_channel_, &NetworkChannel::disconnected, session_, &ClientSession::closeSession);
    connect(session_, &ClientSession::started, network_channel_, &NetworkChannel::start);

    // When closing the session (closing the window), close the status dialog.
    connect(session_, &ClientSession::closedByUser, this, &Client::onSessionClosedByUser);

    // If an error occurs in the session, add a message to the status dialog and stop the channel.
    connect(session_, &ClientSession::errorOccurred,
            this, &Client::onSessionError,
            Qt::QueuedConnection);

    status_dialog_->addStatus(tr("Session started."));
    status_dialog_->hide();

    session_->startSession();
}

void Client::onChannelDisconnected()
{
    status_dialog_->addStatus(tr("Disconnected."));
}

void Client::onChannelError(NetworkChannel::Error error)
{
    const char* message;

    switch (error)
    {
        case NetworkChannel::Error::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case NetworkChannel::Error::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case NetworkChannel::Error::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case NetworkChannel::Error::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case NetworkChannel::Error::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case NetworkChannel::Error::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        case NetworkChannel::Error::PROTOCOL_FAILURE:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case NetworkChannel::Error::ENCRYPTION_FAILURE:
            message = QT_TR_NOOP("An error occurred while encrypting the message.");
            break;

        case NetworkChannel::Error::DECRYPTION_FAILURE:
            message = QT_TR_NOOP("An error occurred while decrypting the message.");
            break;

        case NetworkChannel::Error::AUTHENTICATION_FAILURE:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case NetworkChannel::Error::SESSION_TYPE_NOT_ALLOWED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    status_dialog_->addStatus(tr(message));
}

void Client::onSessionClosedByUser()
{
    status_dialog_->show();
    status_dialog_->close();
}

void Client::onSessionError(const QString& message)
{
    status_dialog_->addStatus(message);
    network_channel_->stop();
}

} // namespace aspia
