//
// PROJECT:         Aspia
// FILE:            host/host.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_H
#define _ASPIA_HOST__HOST_H

#include "base/synchronization/waitable_timer.h"
#include "network/network_channel.h"
#include "network/network_channel_proxy.h"
#include "host/host_session.h"
#include "proto/auth_session.pb.h"

namespace aspia {

class Host
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnSessionTerminate() = 0;
    };

    Host(std::shared_ptr<NetworkChannel> channel, Delegate* delegate);
    ~Host();

    bool IsTerminatedSession() const;

private:
    void OnNetworkChannelStatusChange(NetworkChannel::Status status);
    void OnAuthResultSended(proto::auth::SessionType session_type, proto::auth::Status status);
    void DoAuthorize(IOBuffer& buffer);

    std::shared_ptr<NetworkChannel> channel_;
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    std::unique_ptr<HostSession> session_;

    Delegate* delegate_;

    WaitableTimer auth_timer_;

    DISALLOW_COPY_AND_ASSIGN(Host);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_H
