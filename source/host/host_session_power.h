//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_power.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_POWER_H
#define _ASPIA_HOST__HOST_SESSION_POWER_H

#include "host/host_session.h"
#include "base/message_loop/message_loop_thread.h"
#include "network/network_channel_proxy.h"
#include "proto/power_session.pb.h"

namespace aspia {

class HostSessionPower :
    public HostSession,
    private MessageLoopThread::Delegate
{
public:
    ~HostSessionPower();

    static std::unique_ptr<HostSessionPower> Create(
        std::shared_ptr<NetworkChannelProxy> channel_proxy);

    void OnMessageReceive(const IOBuffer& buffer);

private:
    explicit HostSessionPower(std::shared_ptr<NetworkChannelProxy> channel_proxy);

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void Inject(std::shared_ptr<proto::PowerEvent> power_event);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionPower);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_POWER_H
