//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_power_manage.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_power_manage.h"
#include "protocol/message_serialization.h"
#include "proto/power_session.pb.h"
#include "ui/power_manage_dialog.h"

namespace aspia {

ClientSessionPowerManage::ClientSessionPowerManage(
    const ClientConfig& config,
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
    UiPowerManageDialog dialog;

    proto::PowerEvent::Action action =
        static_cast<proto::PowerEvent::Action>(dialog.DoModal(nullptr));

    if (action != proto::PowerEvent::UNKNOWN)
    {
        proto::power::ClientToHost message;
        message.mutable_power_event()->set_action(action);

        channel_proxy_->Send(SerializeMessage<IOBuffer>(message));
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
