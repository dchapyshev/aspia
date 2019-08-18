//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client_channel.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/version.h"
#include "client/client_authenticator_srp.h"
#include "net/network_channel_proxy.h"
#include "qt_base/application.h"

#include <QApplication>
#include <QEvent>

namespace client {

namespace {

class MessageEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 1;

    MessageEvent(base::ByteArray&& buffer)
        : QEvent(QEvent::Type(kType)),
          buffer(std::move(buffer))
    {
        // Nothing
    }

    base::ByteArray buffer;

private:
    DISALLOW_COPY_AND_ASSIGN(MessageEvent);
};

class ConnectEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 2;

    ConnectEvent()
        : QEvent(QEvent::Type(kType))
    {
        // Nothing
    }

private:
    DISALLOW_COPY_AND_ASSIGN(ConnectEvent);
};

class DisconnectEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 3;

    DisconnectEvent()
        : QEvent(QEvent::Type(kType))
    {
        // Nothing
    }

private:
    DISALLOW_COPY_AND_ASSIGN(DisconnectEvent);
};

class ErrorEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 4;

    ErrorEvent(const QString& message)
        : QEvent(QEvent::Type(kType)),
          message(message)
    {
        // Nothing
    }

    QString message;

private:
    DISALLOW_COPY_AND_ASSIGN(ErrorEvent);
};

} // namespace

Channel::Channel(QObject* parent)
    : QObject(parent)
{
    io_runner_ = qt_base::Application::ioRunner();
    DCHECK(io_runner_);

    asio::io_context* io_context = qt_base::Application::ioContext();
    DCHECK(io_context);

    channel_ = std::make_unique<net::Channel>(*io_context);
    channel_proxy_ = channel_->channelProxy();
}

Channel::~Channel()
{
    channel_proxy_->setListener(nullptr);
    channel_.reset();
}

void Channel::connectToHost(std::u16string_view address, uint16_t port,
                            std::u16string_view username, std::u16string_view password,
                            proto::SessionType session_type)
{
    std::unique_ptr<AuthenticatorSrp> authenticator = std::make_unique<AuthenticatorSrp>();

    authenticator->setSessionType(session_type);
    authenticator->setUserName(username);
    authenticator->setPassword(password);

    authenticator_ = std::move(authenticator);

    channel_proxy_->setListener(this);
    channel_proxy_->connect(address, port);
}

void Channel::disconnectFromHost()
{
    if (!io_runner_->belongsToCurrentThread())
    {
        io_runner_->postTask(std::bind(&Channel::disconnectFromHost, this));
        return;
    }

    channel_proxy_->disconnect();
}

void Channel::resume()
{
    if (!io_runner_->belongsToCurrentThread())
    {
        io_runner_->postTask(std::bind(&Channel::resume, this));
        return;
    }

    channel_proxy_->resume();
}

void Channel::send(base::ByteArray&& buffer)
{
    channel_proxy_->send(std::move(buffer));
}

const base::Version& Channel::peerVersion() const
{
    return authenticator_->peerVersion();
}

void Channel::customEvent(QEvent* event)
{
    switch (event->type())
    {
        case MessageEvent::kType:
            emit messageReceived(reinterpret_cast<MessageEvent*>(event)->buffer);
            break;

        case ConnectEvent::kType:
            emit connected();
            break;

        case DisconnectEvent::kType:
            emit disconnected();
            break;

        case ErrorEvent::kType:
            emit errorOccurred(reinterpret_cast<ErrorEvent*>(event)->message);
            break;

        default:
            break;
    }
}

void Channel::onNetworkConnected()
{
    authenticator_->start(channel_proxy_, [this](Authenticator::Result result)
    {
        channel_proxy_->setListener(this);

        if (result != Authenticator::Result::SUCCESS)
        {
            QApplication::postEvent(this, new ErrorEvent(errorToString(result)));
            return;
        }

        QApplication::postEvent(this, new ConnectEvent());
    });
}

void Channel::onNetworkDisconnected()
{
    QApplication::postEvent(this, new DisconnectEvent());
}

void Channel::onNetworkError(net::ErrorCode error)
{
    QApplication::postEvent(this, new ErrorEvent(errorToString(error)));
}

void Channel::onNetworkMessage(base::ByteArray& buffer)
{
    QApplication::postEvent(this, new MessageEvent(std::move(buffer)));
}

// static
QString Channel::errorToString(net::ErrorCode error)
{
    const char* message;

    switch (error)
    {
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
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    return tr(message);
}

// static
QString Channel::errorToString(Authenticator::Result result)
{
    const char* message;

    switch (result)
    {
        case Authenticator::Result::SUCCESS:
            message = QT_TR_NOOP("Authentication successfully completed.");
            break;

        case Authenticator::Result::NETWORK_ERROR:
            message = QT_TR_NOOP("Network authentication error.");
            break;

        case Authenticator::Result::PROTOCOL_ERROR:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case Authenticator::Result::ACCESS_DENIED:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case Authenticator::Result::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    return tr(message);
}

} // namespace client
