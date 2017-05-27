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

static const size_t kMaximumMessageSize = 5 * 1024 * 1024; // 5 MB

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
    uint32_t size;

    if (!ReadData(reinterpret_cast<uint8_t*>(&size), sizeof(size)))
        return 0;

#if (ARCH_CPU_LITTLE_ENDIAN == 1)
    // Convert from network byte order (big endian).
    size = _byteswap_ulong(size);
#endif

    if (size > kMaximumMessageSize)
        return 0;

    return size;
}

bool NetworkChannel::WriteMessage(const IOBuffer& buffer)
{
    if (buffer.IsEmpty())
        return false;

    if (buffer.size() > kMaximumMessageSize)
        return false;

    uint32_t size = static_cast<uint32_t>(buffer.size());

#if (ARCH_CPU_LITTLE_ENDIAN == 1)
    // Convert to network byte order (big endian).
    size = _byteswap_ulong(size);
#endif

    if (!WriteData(reinterpret_cast<const uint8_t*>(&size), sizeof(size)))
        return false;

    return WriteData(buffer.data(), buffer.size());
}

} // namespace aspia
