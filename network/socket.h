/*
* PROJECT:         Aspia Remote Desktop
* FILE:            network/socket.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_NETWORK__SOCKET_H
#define _ASPIA_NETWORK__SOCKET_H

#include "aspia_config.h"

#include <string>
#include <stdint.h>

#include "base/macros.h"

namespace aspia {

class Socket
{
public:
    Socket() {}
    virtual ~Socket() {}

    virtual void Connect(const std::string &address, int port) = 0;
    virtual void Bind(int port) = 0;
    virtual void Listen() = 0;
    virtual std::unique_ptr<Socket> Accept() = 0;
    virtual void Shutdown() = 0;

    virtual void WriteMessage(const uint8_t *buf, uint32_t len) = 0;
    virtual uint32_t ReadMessageSize() = 0;
    virtual void ReadMessage(uint8_t *buf, uint32_t len) = 0;

private:
    DISALLOW_COPY_AND_ASSIGN(Socket);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SOCKET_H
