//
// PROJECT:         Aspia
// FILE:            network/network_client_tcp.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H
#define _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H

#include "network/network_channel_tcp.h"

namespace aspia {

class NetworkClientTcp
{
public:
    using ConnectCallback = std::function<void(std::shared_ptr<NetworkChannel> channel)>;

    NetworkClientTcp(const std::wstring& address, uint32_t port, ConnectCallback connect_callback);
    ~NetworkClientTcp();

    static bool IsValidHostName(const std::wstring& hostname);
    static bool IsValidPort(uint32_t port);

private:
    void OnResolve(const std::error_code& code,
                   asio::ip::tcp::resolver::iterator endpoint_iterator);

    void OnConnect(const std::error_code& code);
    void DoStop();

    ConnectCallback connect_callback_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;
    std::unique_ptr<NetworkChannelTcp> channel_;
    std::mutex channel_lock_;
    bool terminating_ = false;

    DISALLOW_COPY_AND_ASSIGN(NetworkClientTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H
