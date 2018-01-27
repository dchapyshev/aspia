//
// PROJECT:         Aspia
// FILE:            client/client_session_power_manage.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
#define _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H

#include "client/client_session.h"
#include "base/threading/thread.h"

namespace aspia {

class ClientSessionPowerManage :
    public ClientSession,
    private Thread::Delegate
{
public:
    ClientSessionPowerManage(const proto::Computer& computer,
                             std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~ClientSessionPowerManage();

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnCommandSended();

    Thread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionPowerManage);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
