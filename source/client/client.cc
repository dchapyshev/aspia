//
// PROJECT:         Aspia
// FILE:            client/client.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"

#include <QCryptographicHash>
#include <QMessageBox>

#include "client/ui/authorization_dialog.h"
#include "client/ui/status_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"

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

    // When connected over a network.
    connect(channel_, SIGNAL(channelConnected()), this, SLOT(onChannelConnected()));

    // When a network error occurs.
    connect(channel_, SIGNAL(channelError(const QString&)),
            this, SLOT(onChannelError(const QString&)));

    // The first message from the host is an authorization request.
    connect(channel_, SIGNAL(channelMessage(const QByteArray&)),
            this, SLOT(onAuthorizationRequest(const QByteArray&)));

    // If the network connection is disconnected.
    connect(channel_, SIGNAL(channelDisconnected()), this, SLOT(onChannelDisconnected()));

    // When the status dialog is finished, we call the client's termination.
    connect(status_dialog_, SIGNAL(finished(int)), this, SIGNAL(clientTerminated()));

    // When the status dialog is finished, we stop the connection.
    connect(status_dialog_, SIGNAL(finished(int)), channel_, SLOT(stopChannel()));

    // Delete the dialog after the finish.
    connect(status_dialog_, SIGNAL(finished(int)), status_dialog_, SLOT(deleteLater()));

    QString address = QString::fromUtf8(computer_.address().c_str(), computer_.address().size());
    int port = computer_.port();

    status_dialog_->show();
    status_dialog_->activateWindow();

    status_dialog_->addStatus(tr("Attempt to connect to %1:%2.").arg(address).arg(port));
    channel_->connectToHost(address, port);
}

void Client::onChannelConnected()
{
    status_dialog_->addStatus(tr("Connection established."));
}

void Client::onChannelDisconnected()
{
    status_dialog_->addStatus(tr("Disconnected."));
}

void Client::onChannelError(const QString& message)
{
    status_dialog_->addStatus(tr("Network error: %1.").arg(message));
}

void Client::onAuthorizationRequest(const QByteArray& buffer)
{
    status_dialog_->addStatus(tr("Authorization request received."));

    proto::auth::Request request;
    if (!ParseMessage(buffer, request))
    {
        status_dialog_->addStatus(tr("Protocol error: Invalid authorization request received."));
        return;
    }

    AuthorizationDialog dialog(&computer_, status_dialog_);
    if (dialog.exec() == QDialog::Rejected)
    {
        status_dialog_->addStatus(tr("Authorization is canceled by the user."));
        return;
    }

    QCryptographicHash password_hash(QCryptographicHash::Sha512);
    password_hash.addData(computer_.password().c_str(), computer_.password().size());

    QCryptographicHash key_hash(QCryptographicHash::Sha512);
    key_hash.addData(request.nonce().c_str(), request.nonce().size());
    key_hash.addData(password_hash.result());

    QByteArray client_key = key_hash.result();

    proto::auth::Response response;
    response.set_session_type(computer_.session_type());
    response.set_username(computer_.username());
    response.set_key(client_key.data(), client_key.size());

    disconnect(channel_, SIGNAL(channelMessage(const QByteArray&)),
               this, SLOT(onAuthorizationRequest(const QByteArray&)));
    connect(channel_, SIGNAL(channelMessage(const QByteArray&)),
            this, SLOT(onAuthorizationResult(const QByteArray&)));

    channel_->writeMessage(SerializeMessage(response));
}

void Client::onAuthorizationResult(const QByteArray& buffer)
{
    proto::auth::Result result;
    if (!ParseMessage(buffer, result))
    {
        status_dialog_->addStatus(tr("Protocol error: Invalid authorization result received."));
        return;
    }

    switch (result.status())
    {
        case proto::auth::STATUS_SUCCESS:
            status_dialog_->addStatus(tr("Authorization successfully passed."));
            break;

        case proto::auth::STATUS_ACCESS_DENIED:
            status_dialog_->addStatus(tr("Authorization error: Access denied."));
            break;

        default:
            status_dialog_->addStatus(tr("Authorization error: Unknown status code."));
            break;
    }

    if (result.status() != proto::auth::STATUS_SUCCESS)
        return;

    disconnect(channel_, SIGNAL(channelMessage(const QByteArray&)),
               this, SLOT(onAuthorizationResult(const QByteArray&)));

    switch (computer_.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            session_ = new ClientSessionDesktopManage(&computer_, this);
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            session_ = new ClientSessionDesktopView(&computer_, this);
            break;

        default:
            status_dialog_->addStatus(tr("Unsupported session type."));
            return;
    }

    // Messages received from the network are sent to the session.
    connect(channel_, SIGNAL(channelMessage(const QByteArray&)),
            session_, SLOT(readMessage(const QByteArray&)));

    connect(session_, SIGNAL(sessionMessage(const QByteArray&)),
            channel_, SLOT(writeMessage(const QByteArray&)));

    connect(channel_, SIGNAL(channelDisconnected()), session_, SLOT(closeSession()));

    // When closing the session (closing the window), close the status dialog.
    connect(session_, SIGNAL(sessionClosed()), channel_, SLOT(stopChannel()));
    connect(session_, SIGNAL(sessionClosed()), status_dialog_, SLOT(close()));

    // If an error occurs in the session, add a message to the status dialog and stop the channel.
    connect(session_, SIGNAL(sessionError(const QString&)),
            status_dialog_, SLOT(addStatus(const QString&)));
    connect(session_, SIGNAL(sessionError(const QString&)),
            channel_, SLOT(stopChannel()));

    status_dialog_->addStatus(tr("Session started."));
    status_dialog_->hide();
}

} // namespace aspia
