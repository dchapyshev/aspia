//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_H
#define _ASPIA_HOST__HOST_SESSION_H

#include <stdint.h>

#include "base/io_buffer.h"
#include "base/logging.h"
#include "base/macros.h"

namespace aspia {

class HostSession
{
public:
    class Delegate
    {
    public:
        virtual void OnSessionMessage(const IOBuffer* message) = 0;
        virtual void OnSessionTerminate() = 0;
    };

    HostSession(Delegate* delegate) : delegate_(delegate)
    {
        DCHECK(delegate_);
    }

    virtual ~HostSession() {}

    virtual void Send(const IOBuffer* buffer) = 0;

protected:
    Delegate* delegate_;

private:
    DISALLOW_COPY_AND_ASSIGN(HostSession);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_H
