//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include "base/io_queue.h"
#include "crypto/encryptor_sodium.h"

namespace aspia {

class NetworkChannelProxy;

class NetworkChannel : private Thread
{
public:
    class Listener
    {
    public:
        virtual void OnNetworkChannelStarted() { }
        virtual void OnNetworkChannelDisconnect() = 0;
        virtual void OnNetworkChannelMessage(const IOBuffer& buffer) = 0;
    };

    ~NetworkChannel();

    void StartListening(Listener* listener);

    std::shared_ptr<NetworkChannelProxy> network_channel_proxy()
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
    bool WriteMessage(const IOBuffer& buffer);

    std::unique_ptr<EncryptorSodium> encryptor_;

private:
    void Run() override;
    void OnIncommingMessage(const IOBuffer& buffer);

    Listener* listener_ = nullptr;
    std::shared_ptr<NetworkChannelProxy> proxy_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
