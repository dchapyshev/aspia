//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_power_manage.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
#define _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H

#include "client/client_session.h"
#include "base/message_loop/message_loop_thread.h"
#include "proto/power_session.pb.h"

namespace aspia {

class ClientSessionPowerManage :
    public ClientSession
{
public:
    ClientSessionPowerManage(const ClientConfig& config,
                             ClientSession::Delegate* delegate);

    ~ClientSessionPowerManage();

private:
    // ClientSession implementation.
    void Send(const IOBuffer& buffer) override;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionPowerManage);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
