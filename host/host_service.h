//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_service.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SERVICE_H
#define _ASPIA_HOST__HOST_SERVICE_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/service.h"
#include "network/server_tcp.h"
#include "host/host.h"

#include <memory>

namespace aspia {

static const WCHAR kHostServiceSwitch[] = L"host-service";
static const WCHAR kInstallHostServiceSwitch[] = L"install-host-service";
static const WCHAR kRemoveHostServiceSwitch[] = L"remove-host-service";

class HostService : public Service
{
public:
    HostService();

    static bool IsInstalled();
    static bool Install();
    static bool Remove();

private:
    void Worker() override;
    void OnStop() override;

    void OnServerEvent(Host::SessionEvent event);

private:
    std::unique_ptr<ServerTCP<Host>> server_;

    DISALLOW_COPY_AND_ASSIGN(HostService);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SERVICE_H
