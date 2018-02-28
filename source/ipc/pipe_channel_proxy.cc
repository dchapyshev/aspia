//
// PROJECT:         Aspia
// FILE:            ipc/pipe_channel_proxy.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_channel_proxy.h"

namespace aspia {

PipeChannelProxy::PipeChannelProxy(PipeChannel* channel) :
    channel_(channel)
{
    // Nothing
}

void PipeChannelProxy::WillDestroyCurrentChannel()
{
    {
        std::scoped_lock<std::mutex> lock(channel_lock_);
        channel_ = nullptr;
    }

    stop_event_.notify_one();
}

bool PipeChannelProxy::Disconnect()
{
    {
        std::scoped_lock<std::mutex> lock(channel_lock_);

        if (!channel_)
            return false;

        channel_->Disconnect();
        channel_ = nullptr;
    }

    stop_event_.notify_one();
    return true;
}

bool PipeChannelProxy::IsDisconnecting() const
{
    std::scoped_lock<std::mutex> lock(channel_lock_);

    if (!channel_)
        return true;

    return channel_->IsDisconnecting();
}

bool PipeChannelProxy::Send(IOBuffer&& buffer,
                            PipeChannel::SendCompleteHandler handler)
{
    std::scoped_lock<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Send(std::move(buffer), std::move(handler));
        return true;
    }

    return false;
}

bool PipeChannelProxy::Send(IOBuffer&& buffer)
{
    std::scoped_lock<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Send(std::move(buffer));
        return true;
    }

    return false;
}

bool PipeChannelProxy::Receive(PipeChannel::ReceiveCompleteHandler handler)
{
    std::scoped_lock<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->Receive(std::move(handler));
        return true;
    }

    return false;
}

void PipeChannelProxy::WaitForDisconnect()
{
    std::unique_lock<std::mutex> lock(channel_lock_);

    while (channel_)
        stop_event_.wait(lock);
}

} // namespace aspia
