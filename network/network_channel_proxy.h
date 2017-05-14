//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_proxy.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H

#include "network/network_channel.h"

#include <mutex>

namespace aspia {

class NetworkChannelProxy
{
public:
    bool Send(const IOBuffer& buffer);
    bool SendAsync(IOBuffer buffer);
    void Disconnect();
    bool IsConnected() const;

private:
    friend class NetworkChannel;

    NetworkChannelProxy(NetworkChannel* channel);

    // Called directly by NetworkChannel::~NetworkChannel.
    void WillDestroyCurrentChannel();

    NetworkChannel* channel_;
    mutable std::mutex channel_lock_;

    std::unique_ptr<IOQueue> outgoing_queue_;
    std::mutex outgoing_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelProxy);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_PROXY_H
