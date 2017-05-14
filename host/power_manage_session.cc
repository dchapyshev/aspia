//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/power_manage_session.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/power_manage_session.h"
#include "host/power_injector.h"
#include "protocol/message_serialization.h"

namespace aspia {

PowerManageSession::PowerManageSession(HostSession::Delegate* delegate) :
    HostSession(delegate)
{
    thread_.Start(MessageLoop::Type::TYPE_DEFAULT, this);
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

PowerManageSession::~PowerManageSession()
{
    thread_.Stop();
}

// static
std::unique_ptr<PowerManageSession> PowerManageSession::Create(HostSession::Delegate* delegate)
{
    return std::unique_ptr<PowerManageSession>(new PowerManageSession(delegate));
}

void PowerManageSession::OnBeforeThreadRunning()
{
    // Nothing
}

void PowerManageSession::OnAfterThreadRunning()
{
    delegate_->OnSessionTerminate();
}

void PowerManageSession::Send(const IOBuffer& buffer)
{
    proto::power::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        std::shared_ptr<proto::PowerEvent> power_event(message.release_power_event());
        runner_->PostTask(std::bind(&PowerManageSession::Inject, this, power_event));
    }
}

void PowerManageSession::Inject(std::shared_ptr<proto::PowerEvent> power_event)
{
    InjectPowerEvent(*power_event);
    thread_.StopSoon();
}

} // namespace aspia
