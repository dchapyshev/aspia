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

#include "ipc/ipc_channel_proxy.h"

namespace ipc {

ChannelProxy::ChannelProxy(Channel* channel)
    : channel_(channel)
{
    // Nothing
}

bool ChannelProxy::setListener(Listener* listener)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->setListener(listener);
    return true;
}

bool ChannelProxy::connectToServer(const QString& channel_name)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->connectToServer(channel_name);
    return true;
}

bool ChannelProxy::isConnected() const
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    return channel_->isConnected();
}

bool ChannelProxy::start()
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->start();
    return true;
}

bool ChannelProxy::send(base::ByteArray&& buffer)
{
    std::scoped_lock lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->send(std::move(buffer));
    return true;
}

void ChannelProxy::willDestroyCurrentChannel()
{
    std::scoped_lock lock(channel_lock_);
    channel_ = nullptr;
}

} // namespace ipc
