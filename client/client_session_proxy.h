//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_proxy.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_PROXY_H
#define _ASPIA_CLIENT__CLIENT_SESSION_PROXY_H

#include "client/client_session.h"

#include <mutex>

namespace aspia {

class ClientSessionProxy
{
public:
    bool Send(const IOBuffer& buffer)
    {
        std::lock_guard<std::mutex> lock(client_session_lock_);

        if (!client_session_)
            return false;

        client_session_->Send(buffer);

        return true;
    }

private:
    friend class ClientSession;

    explicit ClientSessionProxy(ClientSession* client_session) :
        client_session_(client_session)
    {
        // Nothing
    }

    // Called directly by ClientSession::~ClientSession.
    void WillDestroyCurrentClientSession()
    {
        std::lock_guard<std::mutex> lock(client_session_lock_);
        client_session_ = nullptr;
    }

    ClientSession* client_session_;
    std::mutex client_session_lock_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionProxy);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_PROXY_H
