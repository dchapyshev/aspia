//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_system_info.h"

namespace aspia {

ClientSessionSystemInfo::ClientSessionSystemInfo(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    window_.reset(new SystemInfoWindow(this));
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    window_.reset();
}

void ClientSessionSystemInfo::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

} // namespace aspia
