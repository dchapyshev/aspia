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

HostSessionPower::HostSessionPower(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
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
std::unique_ptr<HostSessionPower> HostSessionPower::Create(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSessionPower>(new HostSessionPower(channel_proxy));
}

void HostSessionPower::OnBeforeThreadRunning()
{
    channel_proxy_->Receive(std::bind(
        &HostSessionPower::OnMessageReceive, this, std::placeholders::_1));
}

void HostSessionPower::OnAfterThreadRunning()
{
    channel_proxy_->Disconnect();
}

void HostSessionPower::OnMessageReceive(IOBuffer buffer)
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
