//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_pool.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_POOL_H
#define _ASPIA_HOST__HOST_POOL_H

#include "base/message_loop/message_loop_proxy.h"
#include "host/host.h"
#include "network/network_server_tcp.h"

#include <list>

namespace aspia {

class HostPool : private Host::Delegate
{
public:
    HostPool(std::shared_ptr<MessageLoopProxy> runner);
    ~HostPool();

private:
    void OnChannelConnected(std::shared_ptr<NetworkChannel> channel);

    // Host:Delegate implementation.
    void OnSessionTerminate() override;

    bool terminating_ = false;

    std::shared_ptr<MessageLoopProxy> runner_;
    std::unique_ptr<NetworkServerTcp> network_server_;
    std::list<std::unique_ptr<Host>> session_list_;

    DISALLOW_COPY_AND_ASSIGN(HostPool);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_POOL_H
