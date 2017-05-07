//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel.h"
#include "base/logging.h"

namespace aspia {

NetworkChannel::NetworkChannel() :
    listener_(nullptr)
{
    // Nothing
}

NetworkChannel::~NetworkChannel()
{
    Stop();
}

void NetworkChannel::StartListening(Listener* listener)
{
    listener_ = listener;
    Start();
}

std::unique_ptr<IOBuffer> NetworkChannel::ReadMessage()
{
    size_t message_size = ReadMessageSize();

    if (!message_size)
        return nullptr;

    std::unique_ptr<IOBuffer> buffer(new IOBuffer(message_size));

    if (!ReadData(buffer->Data(), buffer->Size()))
        return nullptr;

    return buffer;
}

void NetworkChannel::Run()
{
    if (KeyExchange())
    {
        listener_->OnNetworkChannelStarted();

        std::unique_ptr<IOBuffer> buffer = ReadMessage();

        if (buffer)
        {
            // We process the first message directly (without use |IOQueue]).
            OnIncommingMessage(buffer.get());

            IOQueue incomming_queue(std::bind(&NetworkChannel::OnIncommingMessage,
                                              this,
                                              std::placeholders::_1));

            while (!IsStopping())
            {
                buffer = ReadMessage();

                if (incomming_queue.Add(std::move(buffer)))
                    continue;

                StopSoon();
            }
        }
    }

    listener_->OnNetworkChannelDisconnect();
}

void NetworkChannel::Send(const IOBuffer* buffer)
{
    std::unique_lock<std::mutex> lock(outgoing_lock_);

    if (!WriteMessage(encryptor_->Encrypt(buffer).get()))
    {
        Close();
    }
}

void NetworkChannel::SendAsync(std::unique_ptr<IOBuffer> buffer)
{
    {
        std::unique_lock<std::mutex> lock(outgoing_queue_lock_);

        if (!outgoing_queue_)
        {
            outgoing_queue_.reset(new IOQueue(std::bind(&NetworkChannel::Send,
                                                        this,
                                                        std::placeholders::_1)));
        }
    }

    if (!outgoing_queue_->Add(std::move(buffer)))
    {
        Close();
    }
}

void NetworkChannel::OnIncommingMessage(const IOBuffer* buffer)
{
    listener_->OnNetworkChannelMessage(decryptor_->Decrypt(buffer).get());
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

bool NetworkChannel::WriteMessage(const IOBuffer* buffer)
{
    if (!buffer)
        return false;

    size_t size = buffer->Size();

    // The maximum message size is 3MB.
    DCHECK(size <= 0x3FFFFF);

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

    return WriteData(buffer->Data(), size);
}

} // namespace aspia
