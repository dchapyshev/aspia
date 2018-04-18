//
// PROJECT:         Aspia
// FILE:            client/client.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"

#include "client/ui/authorization_dialog.h"
#include "client/ui/status_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "client/client_session_file_transfer.h"
#include "client/client_user_authorizer.h"

namespace aspia {

Client::Client(const proto::Computer& computer, QObject* parent)
    : QObject(parent),
      computer_(computer)
{
    // Create a network channel.
    channel_ = new Channel(this);

    // Create a status dialog. It displays all information about the progress of the connection
    // and errors.
    status_dialog_ = new StatusDialog();

    connect(channel_, &Channel::channelConnected, this, &Client::onChannelConnected);
    connect(channel_, &Channel::channelError, this, &Client::onChannelError);
    connect(channel_, &Channel::channelDisconnected, this, &Client::onChannelDisconnected);

    connect(status_dialog_, &StatusDialog::finished, [this](int /* result */)
    {
        // When the status dialog is finished, we call the client's termination.
        clientTerminated();

        // When the status dialog is finished, we stop the connection.
        channel_->stopChannel();

        // Delete the dialog after the finish.
        status_dialog_->deleteLater();
    });

    QString address = QString::fromStdString(computer_.address());
    int port = computer_.port();

    status_dialog_->show();
    status_dialog_->activateWindow();

    status_dialog_->addStatus(tr("Attempt to connect to %1:%2.").arg(address).arg(port));
    channel_->connectToHost(address, port);
}

void Client::onChannelConnected()
{
    status_dialog_->addStatus(tr("Connection established."));

    authorizer_ = new ClientUserAuthorizer(status_dialog_);

    authorizer_->setSessionType(computer_.session_type());
    authorizer_->setUserName(QString::fromStdString(computer_.username()));
    authorizer_->setPassword(QString::fromStdString(computer_.password()));

    // Connect authorizer to network.
    connect(authorizer_, &ClientUserAuthorizer::writeMessage, channel_, &Channel::writeMessage);
    connect(authorizer_, &ClientUserAuthorizer::readMessage, channel_, &Channel::readMessage);
    connect(channel_, &Channel::channelMessage, authorizer_, &ClientUserAuthorizer::messageReceived);
    connect(channel_, &Channel::messageWritten, authorizer_, &ClientUserAuthorizer::messageWritten);
    connect(channel_, &Channel::channelDisconnected, authorizer_, &ClientUserAuthorizer::cancel);

    connect(authorizer_, &ClientUserAuthorizer::errorOccurred,
            status_dialog_, &StatusDialog::addStatus);

    // If successful, we start the session.
    connect(authorizer_, &ClientUserAuthorizer::finished, this, &Client::authorizationFinished);

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

void Client::authorizationFinished(proto::auth::Status status)
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

    switch (computer_.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            session_ = new ClientSessionDesktopManage(&computer_, this);
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            session_ = new ClientSessionDesktopView(&computer_, this);
            break;

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            session_ = new ClientSessionFileTransfer(&computer_, this);
            break;

        default:
            status_dialog_->addStatus(tr("Unsupported session type."));
            return;
    }

    // Messages received from the network are sent to the session.
    connect(channel_, &Channel::channelMessage, [this](const QByteArray& buffer)
    {
        session_->readMessage(buffer);
        channel_->readMessage();
    });

    connect(session_, &ClientSession::sessionMessage, [this](const QByteArray& buffer)
    {
        channel_->writeMessage(-1, buffer);
    });

    connect(channel_, &Channel::channelDisconnected, session_, &ClientSession::closeSession);

    // When closing the session (closing the window), close the status dialog.
    connect(session_, &ClientSession::sessionClosed, channel_, &Channel::stopChannel);
    connect(session_, &ClientSession::sessionClosed, status_dialog_, &StatusDialog::close);

    // If an error occurs in the session, add a message to the status dialog and stop the channel.
    connect(session_, &ClientSession::sessionError, this, [this](const QString& message)
    {
        status_dialog_->addStatus(message);
        channel_->stopChannel();
    });

    status_dialog_->addStatus(tr("Session started."));
    status_dialog_->hide();

    session_->startSession();
    channel_->readMessage();
}

} // namespace aspia
