//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_proxy.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel_proxy.h"

namespace aspia {

NetworkChannelProxy::NetworkChannelProxy(NetworkChannel* channel) :
    channel_(channel)
{
    outgoing_queue_ =
        std::make_unique<IOQueue>(std::bind(&NetworkChannelProxy::Send,
                                            this,
                                            std::placeholders::_1));
}

void NetworkChannelProxy::WillDestroyCurrentChannel()
{
    {
        std::lock_guard<std::mutex> lock(outgoing_queue_lock_);
        outgoing_queue_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(channel_lock_);
        channel_ = nullptr;
    }
}

bool NetworkChannelProxy::Send(const IOBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Send(buffer);
        return true;
    }

    return false;
}

bool NetworkChannelProxy::SendAsync(IOBuffer buffer)
{
    std::lock_guard<std::mutex> lock(outgoing_queue_lock_);

    if (outgoing_queue_)
    {
        outgoing_queue_->Add(std::move(buffer));
        return true;
    }

    return false;
}

void NetworkChannelProxy::Disconnect()
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        if (channel_->IsConnected())
            channel_->Disconnect();
    }
}

bool NetworkChannelProxy::IsConnected() const
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        return channel_->IsConnected();
    }

    return false;
}

} // namespace aspia
