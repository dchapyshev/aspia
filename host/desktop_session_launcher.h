//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_launcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__DESKTOP_SESSION_LAUNCHER_H
#define _ASPIA_HOST__DESKTOP_SESSION_LAUNCHER_H

#include <memory>

#include "base/macros.h"
#include "base/service.h"

namespace aspia {

static const WCHAR kDesktopSessionLauncherSwitch[] = L"desktop-session-launcher";
static const WCHAR kDesktopSessionSwitch[] = L"desktop-session";

class DesktopSessionLauncher : private Service
{
public:
    DesktopSessionLauncher(const std::wstring& service_id);
    ~DesktopSessionLauncher() = default;

    void ExecuteService(uint32_t session_id,
                        const std::wstring& input_channel_id,
                        const std::wstring& output_channel_id);

    static bool LaunchSession(uint32_t session_id,
                              const std::wstring& input_channel_id,
                              const std::wstring& output_channel_id);

private:
    void Worker() override;
    void OnStop() override;

private:
    uint32_t session_id_;
    std::wstring input_channel_id_;
    std::wstring output_channel_id_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionLauncher);
};

} // namespace aspia

#endif // _ASPIA_HOST__DESKTOP_SESSION_LAUNCHER_H
