//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/socket.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

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
    Socket() = default;
    virtual ~Socket() = default;

    virtual void Disconnect() = 0;
    virtual bool Write(const uint8_t* buf, uint32_t len) = 0;
    virtual uint32_t ReadSize() = 0;
    virtual bool Read(uint8_t* buf, uint32_t len) = 0;

private:
    DISALLOW_COPY_AND_ASSIGN(Socket);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SOCKET_H
