//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel_proxy.h"

namespace aspia {

NetworkChannelProxy::NetworkChannelProxy(NetworkChannel* channel) :
    channel_(channel)
{
    // Nothing
}

void NetworkChannelProxy::WillDestroyCurrentChannel()
{
    std::lock_guard<std::mutex> lock(channel_lock_);
    channel_ = nullptr;
}

bool NetworkChannelProxy::Send(std::unique_ptr<IOBuffer> buffer,
                               NetworkChannel::SendCompleteHandler handler)
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Send(std::move(buffer), std::move(handler));
        return true;
    }

    return false;
}

bool NetworkChannelProxy::Send(std::unique_ptr<IOBuffer> buffer)
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Send(std::move(buffer));
        return true;
    }

    return false;
}

bool NetworkChannelProxy::Receive(
    NetworkChannel::ReceiveCompleteHandler handler)
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Receive(std::move(handler));
        return true;
    }

    return false;
}

bool NetworkChannelProxy::Disconnect()
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (!channel_)
        return false;

    channel_->Disconnect();
    return true;
}

bool NetworkChannelProxy::IsDiconnecting() const
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (!channel_)
        return true;

    return channel_->IsDiconnecting();
}

} // namespace aspia
