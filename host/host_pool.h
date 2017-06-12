//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_pool.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_POOL_H
#define _ASPIA_HOST__HOST_POOL_H

#include "host/host.h"
#include "network/network_server_tcp.h"

#include <list>

namespace aspia {

class HostPool :
    private NetworkServerTcp::Delegate,
    private Host::Delegate
{
public:
    HostPool(std::shared_ptr<MessageLoopProxy> runner);
    ~HostPool();

    bool Start();

private:
    // NetworkServerTcp::Delegate implementation.
    void OnChannelConnected(std::unique_ptr<NetworkChannel> channel) override;

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
