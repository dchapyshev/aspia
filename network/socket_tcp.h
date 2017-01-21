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

    void Connect(const std::string &address, int port) override;
    void Bind(int port) override;
    void Listen() override;
    std::unique_ptr<Socket> Accept() override;
    void Shutdown() override;

    void WriteMessage(const uint8_t *buf, uint32_t len) override;
    uint32_t ReadMessageSize() override;
    void ReadMessage(uint8_t *buf, uint32_t len) override;

private:
    explicit SocketTCP(SOCKET sock);

    void SetWriteTimeout(int timeout);

    // Disable or enable Nagle’s algorithm.
    void EnableNagles(bool enable);

    int Read(char *buf, int len);
    int Write(const char *buf, int len); 

    void Reader(char *buf, int len);
    void Writer(const char *buf, int len);

private:
    SOCKET sock_;
    bool ref_;

    Lock shutdown_lock_;

    DISALLOW_COPY_AND_ASSIGN(SocketTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SOCKET_TCP_H
