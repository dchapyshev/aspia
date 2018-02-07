//
// PROJECT:         Aspia
// FILE:            client/client_session.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session.h"

#include "client/client_session_desktop_manage.h"
#include "client/client_session_file_transfer.h"
#include "client/client_session_power_manage.h"
#include "client/client_session_system_info.h"

namespace aspia {

// static
std::unique_ptr<ClientSession> ClientSession::Create(
    const proto::Computer& computer, std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    switch (computer.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            return std::make_unique<ClientSessionDesktopManage>(computer, channel_proxy);

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            return std::make_unique<ClientSessionDesktopView>(computer, channel_proxy);

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            return std::make_unique<ClientSessionFileTransfer>(computer, channel_proxy);

        case proto::auth::SESSION_TYPE_POWER_MANAGE:
            return std::make_unique<ClientSessionPowerManage>(computer, channel_proxy);

        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
            return std::make_unique<ClientSessionSystemInfo>(computer, channel_proxy);

        default:
            LOG(LS_ERROR) << "Invalid session type: " << computer.session_type();
            return nullptr;
    }
}

} // namespace aspia
