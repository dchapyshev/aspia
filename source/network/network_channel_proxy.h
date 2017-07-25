//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H

#include "network/network_channel.h"
#include "protocol/io_queue.h"

namespace aspia {

class NetworkChannelProxy
{
public:
    bool Disconnect();
    bool IsConnected() const;
    bool Send(IOBuffer buffer, NetworkChannel::SendCompleteHandler handler);
    bool Receive(NetworkChannel::ReceiveCompleteHandler handler);

private:
    friend class NetworkChannel;

    explicit NetworkChannelProxy(NetworkChannel* channel);

    // Called directly by NetworkChannel::~NetworkChannel.
    void WillDestroyCurrentChannel();

    NetworkChannel* channel_;
    mutable std::mutex channel_lock_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelProxy);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H
