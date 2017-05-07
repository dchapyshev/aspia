//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include "base/io_buffer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "client/client_config.h"

#include <memory>

namespace aspia {

class ClientSession
{
public:
    class Delegate
    {
    public:
        virtual void OnSessionMessage(std::unique_ptr<IOBuffer> buffer) = 0;
        virtual void OnSessionTerminate() = 0;
    };

    ClientSession(const ClientConfig& config, Delegate* delegate) :
        config_(config),
        delegate_(delegate)
    {
        DCHECK(delegate_);
    }

    virtual ~ClientSession() { }

    virtual void Send(const IOBuffer* buffer) = 0;

protected:
    ClientConfig config_;
    Delegate* delegate_;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
