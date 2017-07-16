//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include "protocol/io_queue.h"
#include "crypto/encryptor.h"

namespace aspia {

class NetworkChannelProxy;

class NetworkChannel : private Thread
{
public:
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

        // Called when the first message is received.
        // If the method returns |false|, the connection will be terminated
        // and a method |OnNetworkChannelDisconnect| will be called.
        virtual bool OnNetworkChannelFirstMessage(const SecureIOBuffer& buffer) = 0;

        // Called when a new message is received.
        virtual void OnNetworkChannelMessage(const IOBuffer& buffer) = 0;
    };

    ~NetworkChannel();

    void StartListening(Listener* listener);

    std::shared_ptr<NetworkChannelProxy> network_channel_proxy() const
    {
        return proxy_;
    }

protected:
    friend class NetworkChannelProxy;

    NetworkChannel();
    void Send(const IOBuffer& buffer);
    bool IsConnected() const;
    void Disconnect();

    virtual bool KeyExchange() = 0;
    virtual bool WriteData(const uint8_t* buffer, size_t size) = 0;
    virtual bool ReadData(uint8_t* buffer, size_t size) = 0;
    virtual void Close() = 0;

    size_t ReadMessageSize();
    IOBuffer ReadMessage();
    bool ReadFirstMessage();
    bool WriteMessage(const IOBuffer& buffer);

    std::unique_ptr<Encryptor> encryptor_;

private:
    void Run() override;
    void OnIncommingMessage(const IOBuffer& buffer);

    Listener* listener_ = nullptr;
    std::shared_ptr<NetworkChannelProxy> proxy_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
