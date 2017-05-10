//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_server_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
#define _ASPIA_NETWORK__NETWORK_SERVER_TCP_H

#include <memory>

#include "base/message_loop/message_loop_proxy.h"
#include "base/object_watcher.h"
#include "network/network_channel_tcp.h"
#include "network/firewall_manager.h"
#include "network/socket.h"

namespace aspia {

class NetworkServerTcp : private ObjectWatcher::Delegate
{
public:
    NetworkServerTcp() = default;
    ~NetworkServerTcp();

    class Delegate
    {
    public:
        virtual void OnChannelConnected(std::unique_ptr<NetworkChannel> channel) = 0;
    };

    bool Start(uint16_t port, Delegate* delegate);
    bool IsStarted() const;

private:
    // ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override;

    std::shared_ptr<MessageLoopProxy> runner_;

    ObjectWatcher accept_watcher_;
    Delegate* delegate_ = nullptr;

    FirewallManager firewall_manager_;
    bool firewall_rule_exists_ = false;

    uint16_t port_ = 0;
    Socket server_socket_;
    WsaWaitableEvent accept_event_;

    DISALLOW_COPY_AND_ASSIGN(NetworkServerTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
