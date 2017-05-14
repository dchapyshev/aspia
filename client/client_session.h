//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include "base/io_buffer.h"
#include "client/client_config.h"

namespace aspia {

class ClientSession
{
public:
    class Delegate
    {
    public:
        virtual void OnSessionMessageAsync(IOBuffer buffer) = 0;
        virtual void OnSessionMessage(const IOBuffer& buffer) = 0;
        virtual void OnSessionTerminate() = 0;
    };

    ClientSession(const ClientConfig& config, Delegate* delegate) :
        config_(config),
        delegate_(delegate)
    {
        DCHECK(delegate_);
    }

    virtual ~ClientSession() = default;

    virtual void Send(const IOBuffer& buffer) = 0;

protected:
    ClientConfig config_;
    Delegate* delegate_;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
