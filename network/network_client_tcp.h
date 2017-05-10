//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_client_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H
#define _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H

#include <string>

#include "base/message_loop/message_loop_proxy.h"
#include "base/object_watcher.h"
#include "network/network_channel_tcp.h"

namespace aspia {

class NetworkClientTcp : private ObjectWatcher::Delegate
{
public:
    NetworkClientTcp() = default;
    ~NetworkClientTcp();

    class Delegate
    {
    public:
        virtual void OnConnectionSuccess(std::unique_ptr<NetworkChannel> channel) = 0;
        virtual void OnConnectionTimeout() = 0;
        virtual void OnConnectionError() = 0;
    };

    bool Connect(const std::wstring& address, uint16_t port, Delegate* delegate);

    static bool IsValidHostName(const std::wstring& hostname);
    static bool IsValidPort(uint16_t port);

private:
    // ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override;
    void OnObjectTimeout(HANDLE object) override;

    std::shared_ptr<MessageLoopProxy> runner_;

    ObjectWatcher connect_watcher_;
    Delegate* delegate_ = nullptr;

    Socket socket_;
    WsaWaitableEvent connect_event_;

    DISALLOW_COPY_AND_ASSIGN(NetworkClientTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CLIENT_TCP_H
