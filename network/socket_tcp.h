//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/socket_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SOCKET_TCP_H
#define _ASPIA_NETWORK__SOCKET_TCP_H

#include "aspia_config.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <memory>

#include "base/macros.h"
#include "base/lock.h"
#include "network/socket.h"

namespace aspia {

class SocketTCP : public Socket
{
public:
    SocketTCP();
    ~SocketTCP();

    static SocketTCP* Create();

    bool Connect(const std::string& address, uint16_t port);
    bool Bind(uint16_t port);
    SocketTCP* Accept();

    void Disconnect() override;
    bool Write(const uint8_t* buf, uint32_t len) override;
    uint32_t ReadSize() override;
    bool Read(uint8_t* buf, uint32_t len) override;

private:
    SocketTCP(SOCKET sock, bool ref);

    static bool SetWriteTimeout(SOCKET sock, int timeout);

    // Disable or enable Nagle’s algorithm.
    static bool EnableNagles(SOCKET sock, bool enable);

    bool Writer(const char* buf, int len);

private:
    SOCKET sock_;
    bool ref_;

    Lock shutdown_lock_;

    DISALLOW_COPY_AND_ASSIGN(SocketTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SOCKET_TCP_H
