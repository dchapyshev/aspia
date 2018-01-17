//
// PROJECT:         Aspia
// FILE:            base/host_session_power.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_power.h"
#include "host/power_injector.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"
#include "ui/power/power_session_dialog.h"

namespace aspia {

void HostSessionPower::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (ipc_channel_)
    {
        ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

        uint32_t user_data = GetCurrentProcessId();

        if (ipc_channel_->Connect(user_data))
        {
            OnIpcChannelConnect(user_data);
            ipc_channel_proxy_->WaitForDisconnect();
        }

        ipc_channel_.reset();
    }
}

void HostSessionPower::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(user_data);

    if (session_type != proto::auth::SESSION_TYPE_POWER_MANAGE)
    {
        LOG(FATAL) << "Invalid session type passed: " << session_type;
        return;
    }

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionPower::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionPower::OnIpcChannelMessage(const IOBuffer& buffer)
{
    proto::power::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        switch (message.power_event().action())
        {
            case proto::PowerEvent::SHUTDOWN:
            case proto::PowerEvent::REBOOT:
            case proto::PowerEvent::HIBERNATE:
            case proto::PowerEvent::SUSPEND:
            {
                PowerSessionDialog session_dialog(message.power_event().action());

                PowerSessionDialog::Result result =
                    static_cast<PowerSessionDialog::Result>(session_dialog.DoModal());

                if (result == PowerSessionDialog::Result::EXECUTE)
                {
                    InjectPowerEvent(message.power_event());
                }
            }
            break;

            default:
            {
                LOG(ERROR) << "Unknown power action: " << message.power_event().action();
            }
            break;
        }
    }

    ipc_channel_proxy_->Disconnect();
}

} // namespace aspia
