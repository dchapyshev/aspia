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

#include "client/client.h"

#include "base/logging.h"
#include "build/version.h"
#include "client/config_factory.h"
#include "client/client_authenticator_srp.h"
#include "net/network_channel_proxy.h"

namespace client {

Client::Client(const ConnectData& connect_data, QObject* parent)
    : QObject(parent),
      connect_data_(connect_data)
{
    ConfigFactory::fixupDesktopConfig(&connect_data_.desktop_config);

    channel_ = std::make_unique<net::Channel>();
    channel_proxy_ = channel_->channelProxy();
}

Client::~Client() = default;

void Client::start()
{
    channel_proxy_->setListener(this);
    channel_proxy_->connectToHost(connect_data_.address, connect_data_.port);
}

base::Version Client::peerVersion() const
{
    if (!authenticator_)
        return base::Version();

    return authenticator_->peerVersion();
}

base::Version Client::version() const
{
    return base::Version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);
}

void Client::sendMessage(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return;
    }

    base::ByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(buffer.data());

    channel_proxy_->send(std::move(buffer));
}

void Client::onNetworkConnected()
{
    std::unique_ptr<AuthenticatorSrp> authenticator = std::make_unique<AuthenticatorSrp>();

    authenticator->setSessionType(connect_data_.session_type);
    authenticator->setUserName(connect_data_.username);
    authenticator->setPassword(connect_data_.password);

    authenticator_ = std::move(authenticator);

    authenticator_->start(channel_->channelProxy(), [this](Authenticator::Result result)
    {
        channel_proxy_->setListener(this);

        if (result != Authenticator::Result::SUCCESS)
        {
            emit errorOccurred(errorToString(result));
            return;
        }

        channel_proxy_->resume();
        emit started();
    });
}

void Client::onNetworkDisconnected()
{
    // Nothing
}

void Client::onNetworkError(net::ErrorCode error_code)
{
    emit errorOccurred(errorToString(error_code));
}

// static
QString Client::errorToString(net::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
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
QString Client::errorToString(Authenticator::Result result)
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
