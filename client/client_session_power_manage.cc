//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_power_manage.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_power_manage.h"
#include "protocol/message_serialization.h"
#include "ui/power_manage_dialog.h"

namespace aspia {

ClientSessionPowerManage::ClientSessionPowerManage(const ClientConfig& config,
                                                   ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    PowerManageDialog dialog;

    proto::PowerEvent::Action action =
        static_cast<proto::PowerEvent::Action>(dialog.DoModal(nullptr));

    if (action != proto::PowerEvent::UNKNOWN)
    {
        proto::power::ClientToHost message;
        message.mutable_power_event()->set_action(action);

        IOBuffer buffer(SerializeMessage(message));

        delegate_->OnSessionMessage(std::move(buffer));
    }

    delegate_->OnSessionTerminate();
}

ClientSessionPowerManage::~ClientSessionPowerManage()
{
    // Nothing
}

void ClientSessionPowerManage::Send(const IOBuffer& buffer)
{
    // The power management session does not receive any messages
}

} // namespace aspia
