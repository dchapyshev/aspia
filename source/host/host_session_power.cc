//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_power.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_power.h"
#include "host/power_injector.h"
#include "protocol/message_serialization.h"

namespace aspia {

HostSessionPower::HostSessionPower(HostSession::Delegate* delegate) :
    HostSession(delegate)
{
    thread_.Start(MessageLoop::Type::TYPE_DEFAULT, this);
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

HostSessionPower::~HostSessionPower()
{
    thread_.Stop();
}

// static
std::unique_ptr<HostSessionPower> HostSessionPower::Create(HostSession::Delegate* delegate)
{
    return std::unique_ptr<HostSessionPower>(new HostSessionPower(delegate));
}

void HostSessionPower::OnBeforeThreadRunning()
{
    // Nothing
}

void HostSessionPower::OnAfterThreadRunning()
{
    delegate_->OnSessionTerminate();
}

void HostSessionPower::Send(IOBuffer buffer)
{
    proto::power::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        std::shared_ptr<proto::PowerEvent> power_event(message.release_power_event());
        runner_->PostTask(std::bind(&HostSessionPower::Inject, this, power_event));
    }
}

void HostSessionPower::Inject(std::shared_ptr<proto::PowerEvent> power_event)
{
    InjectPowerEvent(*power_event);
    thread_.StopSoon();
}

} // namespace aspia
