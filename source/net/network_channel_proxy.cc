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

#include "net/network_channel_proxy.h"

#include "crypto/cryptor.h"

namespace net {

ChannelProxy::ChannelProxy(Channel* channel)
    : channel_(channel)
{
    // Nothing
}

bool ChannelProxy::connectToHost(const QString& address, uint16_t port)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->connectToHost(address, port);
    return true;
}

bool ChannelProxy::disconnectFromHost()
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->disconnectFromHost();
    return true;
}

bool ChannelProxy::setListener(Listener* listener)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->setListener(listener);
    return true;
}

bool ChannelProxy::setCryptor(std::unique_ptr<crypto::Cryptor> cryptor)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->setCryptor(std::move(cryptor));
    return true;
}

bool ChannelProxy::isConnected() const
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    return channel_->isConnected();
}

bool ChannelProxy::isPaused() const
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    return channel_->isPaused();
}

bool ChannelProxy::pause()
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->pause();
    return true;
}

bool ChannelProxy::resume()
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->resume();
    return true;
}

QString ChannelProxy::peerAddress() const
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return QString();

    return channel_->peerAddress();
}

bool ChannelProxy::send(const QByteArray& buffer)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->send(buffer);
    return true;
}

void ChannelProxy::willDestroyCurrentChannel()
{
    std::scoped_lock lock(channel_lock_);
    channel_ = nullptr;
}

} // namespace net
