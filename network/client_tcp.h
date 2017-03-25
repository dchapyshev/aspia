//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/client_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__CLIENT_TCP_H
#define _ASPIA_NETWORK__CLIENT_TCP_H

#include <string>

#include "base/thread.h"
#include "base/macros.h"
#include "network/socket_tcp.h"

namespace aspia {

class ClientTCP : public Thread
{
public:
    ClientTCP() : port_(0)
    {
        // Nothing
    }

    virtual ~ClientTCP()
    {
        Stop();
        Wait();
    }

    void Connect(const std::string& address, uint16_t port)
    {
        address_ = address;
        port_ = port;

        Start();
    }

    virtual void OnConnectionSuccess(std::unique_ptr<Socket> sock) = 0;
    virtual void OnConnectionError() = 0;

private:
    void Worker()
    {
        sock_.reset(SocketTCP::Create());

        if (!sock_ || !sock_->Connect(address_, port_))
        {
            if (!IsTerminating())
                OnConnectionError();

            return;
        }

        OnConnectionSuccess(std::move(sock_));
    }

    void OnStop()
    {
        if (sock_)
            sock_->Disconnect();
    }

private:
    std::string address_;
    uint16_t port_;

    std::unique_ptr<SocketTCP> sock_;

    DISALLOW_COPY_AND_ASSIGN(ClientTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__CLIENT_TCP_H
