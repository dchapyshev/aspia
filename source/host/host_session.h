//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_H
#define _ASPIA_HOST__HOST_SESSION_H

#include "protocol/io_buffer.h"

namespace aspia {

class HostSessionProxy;

class HostSession
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnSessionMessage(const IOBuffer& message) = 0;
        virtual void OnSessionTerminate() = 0;
    };

    HostSession(Delegate* delegate);
    virtual ~HostSession();

    std::shared_ptr<HostSessionProxy> host_session_proxy();

protected:
    Delegate* delegate_;

private:
    friend class HostSessionProxy;

    virtual void Send(const IOBuffer& buffer) = 0;

    std::shared_ptr<HostSessionProxy> session_proxy_;

    DISALLOW_COPY_AND_ASSIGN(HostSession);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_H
