//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include "protocol/io_buffer.h"
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

    std::shared_ptr<ClientSessionProxy> client_session_proxy();

protected:
    ClientConfig config_;
    Delegate* delegate_;

private:
    friend class ClientSessionProxy;

    virtual void Send(const IOBuffer& buffer) = 0;

    std::shared_ptr<ClientSessionProxy> session_proxy_;

    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
