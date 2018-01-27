//
// PROJECT:         Aspia
// FILE:            network/network_server_tcp.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
#define _ASPIA_NETWORK__NETWORK_SERVER_TCP_H

#include "base/threading/thread.h"
#include "network/network_channel_tcp.h"
#include "network/firewall_manager.h"
#include "network/firewall_manager_legacy.h"

namespace aspia {

class NetworkServerTcp
    : public Thread::Delegate
{
public:
    using ConnectCallback = std::function<void(std::shared_ptr<NetworkChannel> channel)>;

    NetworkServerTcp(uint16_t port, ConnectCallback connect_callback);
    ~NetworkServerTcp();

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnAccept(const std::error_code& code);
    void DoAccept();
    void DoStop();
    void AddFirewallRule();

    Thread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    ConnectCallback connect_callback_;
    uint16_t port_ = 0;

    // For Vista and later.
    std::unique_ptr<FirewallManager> firewall_manager_;

    // For XP/2003.
    std::unique_ptr<FirewallManagerLegacy> firewall_manager_legacy_;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<NetworkChannelTcp> channel_;
    std::mutex channel_lock_;

    DISALLOW_COPY_AND_ASSIGN(NetworkServerTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
