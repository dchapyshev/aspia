//
// PROJECT:         Aspia
// FILE:            client/client_session_power_manage.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_power_manage.h"
#include "protocol/message_serialization.h"
#include "proto/power_session.pb.h"
#include "ui/power/power_manage_dialog.h"

namespace aspia {

ClientSessionPowerManage::ClientSessionPowerManage(
    const proto::ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    ui_thread_.Start(MessageLoop::Type::TYPE_UI, this);
}

ClientSessionPowerManage::~ClientSessionPowerManage()
{
    ui_thread_.Stop();
}

void ClientSessionPowerManage::OnBeforeThreadRunning()
{
    ui_thread_.StopSoon();
}

void ClientSessionPowerManage::OnAfterThreadRunning()
{
    PowerManageDialog dialog;

    proto::power::Command command =
        static_cast<proto::power::Command>(dialog.DoModal(nullptr));

    if (command != proto::power::COMMAND_UNKNOWN)
    {
        proto::power::ClientToHost message;
        message.set_command(command);

        channel_proxy_->Send(SerializeMessage<IOBuffer>(message));
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
