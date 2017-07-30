//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_launcher_service.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_LAUNCHER_SERVICE_H
#define _ASPIA_HOST__HOST_SESSION_LAUNCHER_SERVICE_H

#include "base/service.h"

namespace aspia {

class HostSessionLauncherService : private Service
{
public:
    HostSessionLauncherService(const std::wstring& service_id);
    ~HostSessionLauncherService() = default;

    static bool CreateStarted(uint32_t session_id,
                              const std::wstring& channel_id);

    void RunLauncher(uint32_t session_id, const std::wstring& channel_id);

private:
    void Worker() override;
    void OnStop() override;

    static const uint32_t kInvalidSessionId = 0xFFFFFFFF;

    uint32_t session_id_ = kInvalidSessionId;
    std::wstring channel_id_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionLauncherService);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_SERVICE_H
