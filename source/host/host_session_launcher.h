//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_launcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
#define _ASPIA_HOST__HOST_SESSION_LAUNCHER_H

#include "base/macros.h"
#include "base/service.h"

namespace aspia {

static const WCHAR kDesktopSessionLauncherSwitch[] =
    L"desktop-session-launcher";

static const WCHAR kDesktopSessionSwitch[] = L"desktop-session";
static const WCHAR kFileTransferSessionSwitch[] = L"file-transfer-session";
static const WCHAR kPowerManageSessionSwitch[] = L"power-manage-session";

class HostSessionLauncher : private Service
{
public:
    HostSessionLauncher(const std::wstring& service_id);
    ~HostSessionLauncher() = default;

    void ExecuteService(uint32_t session_id, const std::wstring& channel_id);

private:
    void Worker() override;
    void OnStop() override;

    static const uint32_t kInvalidSessionId = 0xFFFFFFFF;

    uint32_t session_id_ = kInvalidSessionId;
    std::wstring channel_id_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionLauncher);
};

bool LaunchDesktopSession(uint32_t session_id, const std::wstring& channel_id);

bool LaunchFileTransferSession(uint32_t session_id,
                               const std::wstring& channel_id);

bool LaunchPowerManageSession(uint32_t session_id,
                              const std::wstring& channel_id);

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
