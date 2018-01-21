//
// PROJECT:         Aspia
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

    static bool CreateStarted(const std::wstring& launcher_mode,
                              uint32_t session_id,
                              const std::wstring& channel_id);

    void RunLauncher(const std::wstring& launcher_mode,
                     const std::wstring& session_id,
                     const std::wstring& channel_id);

private:
    void Worker() override;
    void OnStop() override;

    std::wstring launcher_mode_;
    uint32_t session_id_;
    std::wstring channel_id_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionLauncherService);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_SERVICE_H
