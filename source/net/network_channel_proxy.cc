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

#include "crypto/message_decryptor.h"
#include "crypto/message_encryptor.h"

namespace net {

ChannelProxy::ChannelProxy(Channel* channel)
    : channel_(channel)
{
    // Nothing
}

void ChannelProxy::connect(std::u16string_view address, uint16_t port)
{
    if (!channel_)
        return;

    channel_->connect(address, port);
}

void ChannelProxy::setListener(Listener* listener)
{
    if (!channel_)
        return;

    channel_->setListener(listener);
}

void ChannelProxy::setEncryptor(std::unique_ptr<crypto::MessageEncryptor> encryptor)
{
    if (!channel_)
        return;

    channel_->setEncryptor(std::move(encryptor));
}

void ChannelProxy::setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor)
{
    if (!channel_)
        return;

    channel_->setDecryptor(std::move(decryptor));
}

bool ChannelProxy::isConnected() const
{
    if (!channel_)
        return false;

    return channel_->isConnected();
}

bool ChannelProxy::isPaused() const
{
    if (!channel_)
        return false;

    return channel_->isPaused();
}

void ChannelProxy::pause()
{
    if (!channel_)
        return;

    channel_->pause();
}

void ChannelProxy::resume()
{
    if (!channel_)
        return;

    channel_->resume();
}

std::u16string ChannelProxy::peerAddress() const
{
    if (!channel_)
        return std::u16string();

    return channel_->peerAddress();
}

void ChannelProxy::send(base::ByteArray&& buffer)
{
    if (!channel_)
        return;

    channel_->send(std::move(buffer));
}

bool ChannelProxy::setNoDelay(bool enable)
{
    if (!channel_)
        return false;

    return channel_->setNoDelay(enable);
}

bool ChannelProxy::setKeepAlive(bool enable,
                                const std::chrono::milliseconds& time,
                                const std::chrono::milliseconds& interval)
{
    if (!channel_)
        return false;

    return channel_->setKeepAlive(enable, time, interval);
}

void ChannelProxy::willDestroyCurrentChannel()
{
    channel_ = nullptr;
}

} // namespace net
