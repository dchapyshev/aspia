//
// PROJECT:         Aspia
// FILE:            host/host.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_H
#define _ASPIA_HOST__HOST_H

#include "base/waitable_timer.h"
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

    void OnAllowMethodsSended();
    void OnSelectMethodReceived(const IOBuffer& buffer);
    void OnRequestSended();
    void OnResponseReceived(const IOBuffer& buffer);
    void OnResultSended(proto::auth::SessionType session_type, proto::auth::Status status);

    std::shared_ptr<NetworkChannel> channel_;
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    std::unique_ptr<HostSession> session_;

    Delegate* delegate_;

    WaitableTimer auth_timer_;
    std::string nonce_;

    DISALLOW_COPY_AND_ASSIGN(Host);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_H
