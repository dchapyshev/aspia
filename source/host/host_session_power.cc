//
// PROJECT:         Aspia
// FILE:            base/host_session_power.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process.h"
#include "host/host_session_power.h"
#include "host/power_injector.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"
#include "ui/power/power_session_dialog.h"

namespace aspia {

namespace {

constexpr std::chrono::seconds kTimeout{ 60 };

} // namespace

void HostSessionPower::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (!ipc_channel_)
        return;

    ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

    uint32_t user_data = Process::Current().Pid();

    if (ipc_channel_->Connect(user_data))
    {
        OnIpcChannelConnect(user_data);

        // Waiting for the connection to close.
        ipc_channel_proxy_->WaitForDisconnect();
    }
}

void HostSessionPower::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(user_data);

    if (session_type != proto::auth::SESSION_TYPE_POWER_MANAGE)
    {
        LOG(LS_FATAL) << "Invalid session type passed: " << session_type;
        return;
    }

    timer_.Start(kTimeout, std::bind(&HostSessionPower::OnSessionTimeout, this));

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionPower::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionPower::OnIpcChannelMessage(const IOBuffer& buffer)
{
    timer_.Stop();

    proto::power::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        switch (message.command())
        {
            case proto::power::COMMAND_SHUTDOWN:
            case proto::power::COMMAND_REBOOT:
            case proto::power::COMMAND_HIBERNATE:
            case proto::power::COMMAND_SUSPEND:
            {
                PowerSessionDialog session_dialog(message.command());

                PowerSessionDialog::Result result =
                    static_cast<PowerSessionDialog::Result>(session_dialog.DoModal());

                if (result == PowerSessionDialog::Result::EXECUTE)
                {
                    InjectPowerCommand(message.command());
                }
            }
            break;

            default:
            {
                LOG(LS_ERROR) << "Unknown power action: " << message.command();
            }
            break;
        }
    }

    ipc_channel_proxy_->Disconnect();
}

void HostSessionPower::OnSessionTimeout()
{
    ipc_channel_proxy_->Disconnect();
}

} // namespace aspia
