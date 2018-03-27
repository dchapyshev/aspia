//
// PROJECT:         Aspia
// FILE:            ipc/pipe_channel_proxy.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CHANNEL_PROXY_H
#define _ASPIA_IPC__PIPE_CHANNEL_PROXY_H

#include <condition_variable>
#include <mutex>

#include "ipc/pipe_channel.h"

namespace aspia {

class PipeChannelProxy
{
public:
    bool Disconnect();
    bool IsDisconnecting() const;
    bool Send(QByteArray&& buffer, PipeChannel::SendCompleteHandler handler);
    bool Send(QByteArray&& buffer);
    bool Receive(PipeChannel::ReceiveCompleteHandler handler);
    void WaitForDisconnect();

private:
    friend class PipeChannel;

    explicit PipeChannelProxy(PipeChannel* channel);

    // Called directly by NetworkChannel::~NetworkChannel.
    void WillDestroyCurrentChannel();

    PipeChannel* channel_;
    mutable std::mutex channel_lock_;

    std::condition_variable stop_event_;

    Q_DISABLE_COPY(PipeChannelProxy);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CHANNEL_PROXY_H
