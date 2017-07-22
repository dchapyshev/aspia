//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include "crypto/secure_io_buffer.h"

namespace aspia {

class NetworkChannelProxy;

class NetworkChannel
{
public:
    NetworkChannel();
    virtual ~NetworkChannel();

    class Listener
    {
    public:
        virtual ~Listener() = default;

        // The method is called when the connection is successfully established.
        virtual void OnNetworkChannelConnect() = 0;

        // Called when the connection is disconnected.
        // The method will be called even if method |OnNetworkChannelConnect|
        // was not called (for example, if an encryption key exchange error occurred).
        virtual void OnNetworkChannelDisconnect() = 0;

        // Called when a new message is received.
        virtual void OnNetworkChannelMessage(const IOBuffer& buffer) = 0;
    };

    virtual void StartListening(Listener* listener) = 0;

    std::shared_ptr<NetworkChannelProxy> network_channel_proxy() const;

protected:
    friend class NetworkChannelProxy;

    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual void Send(const IOBuffer& buffer) = 0;

private:
    std::shared_ptr<NetworkChannelProxy> proxy_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
