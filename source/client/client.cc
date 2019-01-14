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

namespace client {

Client::Client(const ConnectData& connect_data, QObject* parent)
    : QObject(parent),
      connect_data_(connect_data)
{
    // Nothing
}

Client::~Client() = default;

void Client::start()
{
    network_channel_ = new net::ChannelClient(this);

    connect(network_channel_, &net::ChannelClient::connected, this, &Client::started);
    connect(network_channel_, &net::ChannelClient::disconnected, this, &Client::finished);
    connect(network_channel_, &net::ChannelClient::messageReceived, this, &Client::messageReceived);

    connect(network_channel_, &net::ChannelClient::errorOccurred, [this](net::Channel::Error error)
    {
        emit errorOccurred(networkErrorToString(error));
    });

    connect(this, &Client::errorOccurred, network_channel_, &net::ChannelClient::stop);
    connect(this, &Client::started, network_channel_, &net::Channel::start);

    network_channel_->connectToHost(connect_data_.address, connect_data_.port,
                                    connect_data_.username, connect_data_.password,
                                    connect_data_.session_type);
}

QVersionNumber Client::hostVersion() const
{
    if (!network_channel_)
        return QVersionNumber();

    return network_channel_->peerVersion();
}

void Client::sendMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return;
    }

    QByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));

    network_channel_->send(buffer);
}

// static
QString Client::networkErrorToString(net::Channel::Error error)
{
    const char* message;

    switch (error)
    {
        case net::Channel::Error::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case net::Channel::Error::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case net::Channel::Error::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case net::Channel::Error::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case net::Channel::Error::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case net::Channel::Error::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        case net::Channel::Error::PROTOCOL_FAILURE:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case net::Channel::Error::ENCRYPTION_FAILURE:
            message = QT_TR_NOOP("An error occurred while encrypting the message.");
            break;

        case net::Channel::Error::DECRYPTION_FAILURE:
            message = QT_TR_NOOP("An error occurred while decrypting the message.");
            break;

        case net::Channel::Error::AUTHENTICATION_FAILURE:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case net::Channel::Error::SESSION_TYPE_NOT_ALLOWED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    return tr(message);
}

} // namespace client
