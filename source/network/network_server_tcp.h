//
// PROJECT:         Aspia
// FILE:            network/network_server_tcp.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
#define _ASPIA_NETWORK__NETWORK_SERVER_TCP_H

#include "base/threading/thread.h"
#include "network/network_channel_tcp.h"

namespace aspia {

class NetworkServerTcp
    : public Thread::Delegate
{
public:
    using ConnectCallback = std::function<void(std::shared_ptr<NetworkChannel> channel)>;

    NetworkServerTcp(int port, ConnectCallback connect_callback);
    ~NetworkServerTcp();

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnAccept(const std::error_code& code);
    void DoAccept();
    void DoStop();

    Thread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    ConnectCallback connect_callback_;
    int port_ = 0;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<NetworkChannelTcp> channel_;
    std::mutex channel_lock_;

    Q_DISABLE_COPY(NetworkServerTcp)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_TCP_H
