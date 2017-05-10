//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/socket.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SOCKET_H
#define _ASPIA_NETWORK__SOCKET_H

#include "base/macros.h"

#include <winsock2.h>

namespace aspia {

class Socket
{
public:
    Socket() = default;

    explicit Socket(SOCKET socket) : socket_(socket)
    {
        // Nothing
    }

    Socket(Socket&& other)
    {
        socket_ = other.socket_;
        other.socket_ = INVALID_SOCKET;
    }

    ~Socket()
    {
        Close();
    }

    bool IsValid() const
    {
        return socket_ != INVALID_SOCKET;
    }

    SOCKET Handle() const
    {
        return socket_;
    }

    void Reset(SOCKET socket = INVALID_SOCKET)
    {
        Close();
        socket_ = socket;
    }

    Socket& operator=(Socket&& other)
    {
        Reset(other.socket_);
        other.socket_ = INVALID_SOCKET;
        return *this;
    }

    operator SOCKET()
    {
        return Handle();
    }

private:
    void Close()
    {
        if (IsValid())
        {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
    }

    SOCKET socket_ = INVALID_SOCKET;

    DISALLOW_COPY_AND_ASSIGN(Socket);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SOCKET_H
