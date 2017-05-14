//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/power_manage_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__POWER_MANAGE_SESSION_H
#define _ASPIA_HOST__POWER_MANAGE_SESSION_H

#include "host/host_session.h"
#include "base/message_loop/message_loop_thread.h"

namespace aspia {

class PowerManageSession :
    public HostSession,
    private MessageLoopThread::Delegate
{
public:
    ~PowerManageSession();

    static std::unique_ptr<PowerManageSession> Create(HostSession::Delegate* delegate);

    void Send(const IOBuffer& buffer) override;

private:
    PowerManageSession(HostSession::Delegate* delegate);

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(PowerManageSession);
};

} // namespace aspia

#endif // _ASPIA_HOST__POWER_MANAGE_SESSION_H
