//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/client_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__CLIENT_TCP_H
#define _ASPIA_NETWORK__CLIENT_TCP_H

#include <functional>

#include "base/exception.h"
#include "base/thread.h"
#include "base/macros.h"
#include "network/socket_tcp.h"

namespace aspia {

class ClientTCP : public Thread
{
public:
    typedef std::function<void(const char*)> OnErrorCallback;
    typedef std::function<void(std::unique_ptr<Socket>&)> OnConnectedCallback;

    ClientTCP(const std::string &address, int port,
              OnConnectedCallback on_connected_callback,
              OnErrorCallback on_error_callback) :
        on_connected_callback_(on_connected_callback),
        on_error_callback_(on_error_callback),
        address_(address),
        port_(port)
    {
        Start();
    }

    ~ClientTCP()
    {
        if (IsActiveThread())
        {
            Stop();
            WaitForEnd();
        }
    }

private:
    void Worker()
    {
        try
        {
            sock_.reset(new SocketTCP());

            sock_->Connect(address_, port_);

            on_connected_callback_(sock_);
        }
        catch (const Exception &err)
        {
            if (!IsThreadTerminating())
            {
                DLOG(WARNING) << "Exception in tcp client: " << err.What();
                on_error_callback_(err.What());
            }
        }
    }

    void OnStop()
    {
        if (sock_)
        {
            sock_->Shutdown();
        }
    }

private:
    std::string address_;
    int port_;

    std::unique_ptr<Socket> sock_;

    OnConnectedCallback on_connected_callback_;
    OnErrorCallback on_error_callback_;

    DISALLOW_COPY_AND_ASSIGN(ClientTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__CLIENT_TCP_H
