//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel.h"
#include "network/network_channel_proxy.h"
#include "base/logging.h"

namespace aspia {

static const size_t kMaximumMessageSize = 0x3FFFFF; // 3 MB

NetworkChannel::NetworkChannel()
{
    proxy_.reset(new NetworkChannelProxy(this));
}

NetworkChannel::~NetworkChannel()
{
    proxy_->WillDestroyCurrentChannel();
    proxy_ = nullptr;

    Stop();
}

void NetworkChannel::StartListening(Listener* listener)
{
    listener_ = listener;
    Start();
}

bool NetworkChannel::IsConnected() const
{
    return !IsStopping();
}

void NetworkChannel::Disconnect()
{
    StopSoon();
    Close();
}

IOBuffer NetworkChannel::ReadMessage()
{
    size_t message_size = ReadMessageSize();

    if (!message_size)
        return IOBuffer();

    IOBuffer buffer(message_size);

    if (!ReadData(buffer.data(), buffer.size()))
        return IOBuffer();

    return buffer;
}

void NetworkChannel::Run()
{
    if (KeyExchange())
    {
        listener_->OnNetworkChannelStarted();

        IOQueue incomming_queue(std::bind(&NetworkChannel::OnIncommingMessage,
                                          this,
                                          std::placeholders::_1));

        while (!IsStopping())
        {
            IOBuffer buffer(ReadMessage());

            if (!buffer.IsEmpty())
            {
                incomming_queue.Add(std::move(buffer));
                continue;
            }

            StopSoon();
        }
    }

    listener_->OnNetworkChannelDisconnect();
}

void NetworkChannel::Send(const IOBuffer& buffer)
{
    IOBuffer message_buffer = encryptor_->Encrypt(buffer);

    if (!WriteMessage(message_buffer))
    {
        StopSoon();
    }
}

void NetworkChannel::OnIncommingMessage(const IOBuffer& buffer)
{
    IOBuffer message_buffer(encryptor_->Decrypt(buffer));

    if (message_buffer.IsEmpty())
    {
        StopSoon();
        return;
    }

    listener_->OnNetworkChannelMessage(message_buffer);
}

size_t NetworkChannel::ReadMessageSize()
{
    uint8_t byte;

    if (!ReadData(&byte, sizeof(byte)))
        return 0;

    size_t size = byte & 0x7F;

    if (byte & 0x80)
    {
        if (!ReadData(&byte, sizeof(byte)))
            return 0;

        size += (byte & 0x7F) << 7;

        if (byte & 0x80)
        {
            if (!ReadData(&byte, sizeof(byte)))
                return 0;

            size += byte << 14;
        }
    }

    return size;
}

bool NetworkChannel::WriteMessage(const IOBuffer& buffer)
{
    if (buffer.IsEmpty())
        return false;

    const size_t size = buffer.size();

    // The maximum message size is 3MB.
    if (size > kMaximumMessageSize)
        return false;

    uint8_t length[3];
    int count = 1;

    length[0] = size & 0x7F;

    if (size > 0x7F)
    {
        length[0] |= 0x80;
        length[count++] = size >> 7 & 0x7F;

        if (size > 0x3FFF)
        {
            length[1] |= 0x80;
            length[count++] = size >> 14 & 0xFF;
        }
    }

    if (!WriteData(length, count))
        return false;

    return WriteData(buffer.data(), size);
}

} // namespace aspia
