//
// PROJECT:         Aspia
// FILE:            base/host_session_power.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_POWER_H
#define _ASPIA_HOST__HOST_SESSION_POWER_H

#include "base/synchronization/waitable_timer.h"
#include "ipc/pipe_channel.h"

namespace aspia {

class HostSessionPower
{
public:
    HostSessionPower() = default;
    ~HostSessionPower() = default;

    void Run(const std::wstring& channel_id);

private:
    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelMessage(const IOBuffer& buffer);

    void OnSessionTimeout();

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;

    WaitableTimer timer_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionPower);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_POWER_H
