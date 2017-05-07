//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include "base/io_queue.h"
#include "crypto/encryptor.h"
#include "crypto/decryptor.h"

namespace aspia {

class NetworkChannel : public Thread
{
public:
    class Listener
    {
    public:
        virtual void OnNetworkChannelStarted() { }
        virtual void OnNetworkChannelDisconnect() = 0;
        virtual void OnNetworkChannelMessage(const IOBuffer* buffer) = 0;
    };

    ~NetworkChannel();

    void StartListening(Listener* listener);
    void Send(const IOBuffer* buffer);
    void SendAsync(std::unique_ptr<IOBuffer> buffer);
    virtual void Close() = 0;
    virtual bool IsConnected() const = 0;

protected:
    NetworkChannel();

    virtual bool KeyExchange() = 0;
    virtual bool WriteData(const uint8_t* buffer, size_t size) = 0;
    virtual bool ReadData(uint8_t* buffer, size_t size) = 0;

    size_t ReadMessageSize();
    std::unique_ptr<IOBuffer> ReadMessage();
    bool WriteMessage(const IOBuffer* buffer);

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

private:
    void Run() override;

    void OnIncommingMessage(const IOBuffer* buffer);

    Listener* listener_;
    std::unique_ptr<IOQueue> outgoing_queue_;
    std::mutex outgoing_queue_lock_;

    std::mutex outgoing_lock_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
