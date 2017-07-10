//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session.h"
#include "host/host_session_proxy.h"

namespace aspia {

HostSession::HostSession(Delegate* delegate) : delegate_(delegate)
{
    DCHECK(delegate_);
    session_proxy_.reset(new HostSessionProxy(this));
}

HostSession::~HostSession()
{
    session_proxy_->WillDestroyCurrentHostSession();
    session_proxy_ = nullptr;
}

std::shared_ptr<HostSessionProxy> HostSession::host_session_proxy()
{
    return session_proxy_;
}

} // namespace aspia
