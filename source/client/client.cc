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
#include "client/client_channel.h"
#include "net/network_channel_proxy.h"

namespace client {

Client::Client(const ConnectData& connect_data, QObject* parent)
    : QObject(parent),
      connect_data_(connect_data)
{
    ConfigFactory::fixupDesktopConfig(&connect_data_.desktop_config);
}

Client::~Client() = default;

void Client::start()
{
    if (channel_)
        return;

    channel_ = std::make_unique<Channel>();

    connect(channel_.get(), &Channel::connected, [this]()
    {
        channel_->resume();
        emit started();
    });

    connect(channel_.get(), &Channel::messageReceived, this, &Client::onMessageReceived);
    connect(channel_.get(), &Channel::errorOccurred, this, &Client::errorOccurred);

    channel_->connectToHost(connect_data_.address,
                            connect_data_.port,
                            connect_data_.username,
                            connect_data_.password,
                            connect_data_.session_type);
}

base::Version Client::peerVersion() const
{
    if (!channel_)
        return base::Version();

    return channel_->peerVersion();
}

base::Version Client::version() const
{
    return base::Version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);
}

void Client::sendMessage(const google::protobuf::MessageLite& message)
{
    DCHECK(channel_);

    const size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return;
    }

    base::ByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(buffer.data());

    channel_->send(std::move(buffer));
}

} // namespace client
