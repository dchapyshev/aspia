//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CHANNEL_PROXY_H
#define _ASPIA_IPC__PIPE_CHANNEL_PROXY_H

#include "base/synchronization/waitable_event.h"
#include "ipc/pipe_channel.h"

namespace aspia {

class PipeChannelProxy
{
public:
    bool Send(IOBuffer buffer, PipeChannel::SendCompleteHandler handler);
    bool Send(IOBuffer buffer);
    bool Receive(PipeChannel::ReceiveCompleteHandler handler);

    void WaitForDisconnect();

private:
    friend class PipeChannel;

    explicit PipeChannelProxy(PipeChannel* channel);

    // Called directly by NetworkChannel::~NetworkChannel.
    void WillDestroyCurrentChannel();

    PipeChannel* channel_;
    mutable std::mutex channel_lock_;

    WaitableEvent stop_event_ { WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED };

    DISALLOW_COPY_AND_ASSIGN(PipeChannelProxy);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CHANNEL_PROXY_H
