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

#include "client/ui/authorization_dialog.h"
#include "client/ui/status_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "client/client_session_file_transfer.h"
#include "client/client_session_system_info.h"
#include "client/client_user_authorizer.h"

namespace aspia {

Client::Client(const ConnectData& connect_data, QObject* parent)
    : QObject(parent),
      connect_data_(connect_data)
{
    // Create a network channel.
    network_channel_ = NetworkChannel::createClient(this);

    // Create a status dialog. It displays all information about the progress of the connection
    // and errors.
    status_dialog_ = new StatusDialog();

    connect(network_channel_, &NetworkChannel::connected, this, &Client::onChannelConnected);
    connect(network_channel_, &NetworkChannel::errorOccurred, this, &Client::onChannelError);
    connect(network_channel_, &NetworkChannel::disconnected, this, &Client::onChannelDisconnected);

    connect(status_dialog_, &StatusDialog::finished, [this](int /* result */)
    {
        // When the status dialog is finished, we stop the connection.
        network_channel_->stop();

        // Delete the dialog after the finish.
        status_dialog_->deleteLater();

        // When the status dialog is finished, we call the client's termination.
        emit clientTerminated(this);
    });

    QString address = connect_data_.address();
    int port = connect_data_.port();

    status_dialog_->show();
    status_dialog_->activateWindow();

    status_dialog_->addStatus(tr("Attempt to connect to %1:%2.").arg(address).arg(port));
    network_channel_->connectToHost(address, port);
}

void Client::onChannelConnected()
{
    status_dialog_->addStatus(tr("Connection established."));

    authorizer_ = new ClientUserAuthorizer(status_dialog_);

    authorizer_->setSessionType(connect_data_.sessionType());
    authorizer_->setUserName(connect_data_.userName());
    authorizer_->setPassword(connect_data_.password());

    // Connect authorizer to network.
    connect(authorizer_, &ClientUserAuthorizer::writeMessage,
            network_channel_, &NetworkChannel::writeMessage);

    connect(authorizer_, &ClientUserAuthorizer::readMessage,
            network_channel_, &NetworkChannel::readMessage);

    connect(network_channel_, &NetworkChannel::messageReceived,
            authorizer_, &ClientUserAuthorizer::messageReceived);

    connect(network_channel_, &NetworkChannel::messageWritten,
            authorizer_, &ClientUserAuthorizer::messageWritten);

    connect(network_channel_, &NetworkChannel::disconnected,
            authorizer_, &ClientUserAuthorizer::cancel);

    connect(authorizer_, &ClientUserAuthorizer::errorOccurred,
            status_dialog_, &StatusDialog::addStatus);

    // If successful, we start the session.
    connect(authorizer_, &ClientUserAuthorizer::finished, this, &Client::onAuthorizationFinished);

    // Now run authorization.
    status_dialog_->addStatus(tr("Authorization started."));
    authorizer_->start();
}

void Client::onChannelDisconnected()
{
    status_dialog_->addStatus(tr("Disconnected."));
}

void Client::onChannelError(const QString& message)
{
    status_dialog_->addStatus(tr("Network error: %1.").arg(message));
}

void Client::onAuthorizationFinished(proto::auth::Status status)
{
    delete authorizer_;

    switch (status)
    {
        case proto::auth::STATUS_SUCCESS:
            status_dialog_->addStatus(tr("Successful authorization."));
            break;

        case proto::auth::STATUS_ACCESS_DENIED:
            status_dialog_->addStatus(tr("Authorization error: Access denied."));
            return;

        case proto::auth::STATUS_CANCELED:
            status_dialog_->addStatus(tr("Authorization has been canceled."));
            return;

        default:
            status_dialog_->addStatus(tr("Authorization error: Unknown status code."));
            return;
    }

    switch (connect_data_.sessionType())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            session_ = new ClientSessionDesktopManage(&connect_data_, this);
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            session_ = new ClientSessionDesktopView(&connect_data_, this);
            break;

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            session_ = new ClientSessionFileTransfer(&connect_data_, this);
            break;

        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
            session_ = new ClientSessionSystemInfo(&connect_data_, this);
            break;

        default:
            status_dialog_->addStatus(tr("Unsupported session type."));
            return;
    }

    // Messages received from the network are sent to the session.
    connect(session_, &ClientSession::readMessage, network_channel_, &NetworkChannel::readMessage);
    connect(network_channel_, &NetworkChannel::messageReceived, session_, &ClientSession::messageReceived);
    connect(session_, &ClientSession::writeMessage, network_channel_, &NetworkChannel::writeMessage);
    connect(network_channel_, &NetworkChannel::messageWritten, session_, &ClientSession::messageWritten);
    connect(network_channel_, &NetworkChannel::disconnected, session_, &ClientSession::closeSession);

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
