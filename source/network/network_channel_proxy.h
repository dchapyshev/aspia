//
// PROJECT:         Aspia
// FILE:            network/network_channel_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
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
    // Sends |buffer| and executes |handler| after successful sending.
    // |handler| may be absent. This case, instead of the parameter, you
    // must specify |nullptr|.
    bool Send(IOBuffer&& buffer, NetworkChannel::SendCompleteHandler handler);
    bool Send(IOBuffer&& buffer);

    // Asynchronously gives a command to receive data. After the data is
    // received, |handler| is executed.
    bool Receive(NetworkChannel::ReceiveCompleteHandler handler);

    // Asynchronously gives a command to disconnect the channel. It is not
    // guaranteed that after the method is completed, the channel will be
    // immediately stopped.
    bool Disconnect();

    // Checks whether the command to disconnect the channel is given.
    // If the channel does not already exist, |true| will also be returned.
    bool IsDiconnecting() const;

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
