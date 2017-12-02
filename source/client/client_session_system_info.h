//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_system_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H
#define _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H

#include "client/client_session.h"
#include "ui/system_info/system_info_window.h"
#include "ui/system_info/report_creator.h"

namespace aspia {

class ClientSessionSystemInfo
    : public ClientSession,
      private SystemInfoWindow::Delegate
{
public:
    ClientSessionSystemInfo(const ClientConfig& config,
                            std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~ClientSessionSystemInfo();

private:
    // SystemInfoWindow::Delegate implementation.
    void OnWindowClose() override;
    void OnRequest(std::string_view guid,
                   std::shared_ptr<ReportCreatorProxy> report_creator) override;

    void OnRequestSended();
    void OnReplyReceived(const IOBuffer& buffer);

    std::unique_ptr<SystemInfoWindow> window_;
    std::shared_ptr<ReportCreatorProxy> report_creator_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionSystemInfo);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H
