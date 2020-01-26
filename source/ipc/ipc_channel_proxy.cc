//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "ipc/ipc_channel_proxy.h"

namespace ipc {

ChannelProxy::ChannelProxy(Channel* channel)
    : channel_(channel)
{
    // Nothing
}

void ChannelProxy::setListener(Listener* listener)
{
    if (channel_)
        channel_->setListener(listener);
}

bool ChannelProxy::connect(std::u16string_view channel_id)
{
    if (!channel_)
        return false;

    return channel_->connect(channel_id);
}

void ChannelProxy::disconnect()
{
    if (channel_)
        channel_->disconnect();
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
    if (channel_)
        channel_->pause();
}

void ChannelProxy::resume()
{
    if (channel_)
        channel_->resume();
}

void ChannelProxy::send(base::ByteArray&& buffer)
{
    if (channel_)
        channel_->send(std::move(buffer));
}

void ChannelProxy::willDestroyCurrentChannel()
{
    channel_ = nullptr;
}

} // namespace ipc
