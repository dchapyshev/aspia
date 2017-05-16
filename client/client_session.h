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

class ClientSessionProxy;

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

    ClientSession(const ClientConfig& config, Delegate* delegate);
    virtual ~ClientSession();

    virtual void Send(const IOBuffer& buffer) = 0;

    std::shared_ptr<ClientSessionProxy> client_session_proxy();

protected:
    std::shared_ptr<ClientSessionProxy> session_proxy_;
    ClientConfig config_;
    Delegate* delegate_;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
