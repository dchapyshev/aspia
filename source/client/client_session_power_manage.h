//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_power_manage.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
#define _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H

#include "client/client_session.h"
#include "base/message_loop/message_loop_thread.h"

namespace aspia {

class ClientSessionPowerManage :
    public ClientSession,
    private MessageLoopThread::Delegate
{
public:
    ClientSessionPowerManage(
        const ClientConfig& config,
        std::shared_ptr<NetworkChannelProxy> channel_proxy);

    ~ClientSessionPowerManage();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    MessageLoopThread ui_thread_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionPowerManage);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_POWER_MANAGE_H
