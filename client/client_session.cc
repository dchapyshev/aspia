//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session.h"
#include "client/client_session_proxy.h"

namespace aspia {

ClientSession::ClientSession(const ClientConfig& config, Delegate* delegate) :
    config_(config),
    delegate_(delegate)
{
    DCHECK(delegate_);
    session_proxy_.reset(new ClientSessionProxy(this));
}

ClientSession::~ClientSession()
{
    session_proxy_->WillDestroyCurrentClientSession();
    session_proxy_ = nullptr;
}

std::shared_ptr<ClientSessionProxy> ClientSession::client_session_proxy()
{
    return session_proxy_;
}

} // namespace aspia
