//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_PROXY_H
#define _ASPIA_HOST__HOST_SESSION_PROXY_H

#include "host/host_session.h"

#include <mutex>

namespace aspia {

class HostSessionProxy
{
public:
    bool Send(IOBuffer buffer)
    {
        std::lock_guard<std::mutex> lock(host_session_lock_);

        if (!host_session_)
            return false;

        host_session_->Send(std::move(buffer));

        return true;
    }

private:
    friend class HostSession;

    explicit HostSessionProxy(HostSession* host_session) :
        host_session_(host_session)
    {
        // Nothing
    }

    // Called directly by HostSession::~HostSession.
    void WillDestroyCurrentHostSession()
    {
        std::lock_guard<std::mutex> lock(host_session_lock_);
        host_session_ = nullptr;
    }

    HostSession* host_session_;
    std::mutex host_session_lock_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionProxy);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_PROXY_H
