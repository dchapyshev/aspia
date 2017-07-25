//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_launcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CONSOLE_SESSION_LAUNCHER_H
#define _ASPIA_HOST__CONSOLE_SESSION_LAUNCHER_H

#include <memory>

#include "base/macros.h"
#include "base/service.h"

namespace aspia {

static const WCHAR kDesktopSessionLauncherSwitch[] = L"desktop-session-launcher";
static const WCHAR kDesktopSessionSwitch[] = L"desktop-session";
static const WCHAR kFileTransferSessionSwitch[] = L"file-transfer-session";

class ConsoleSessionLauncher : private Service
{
public:
    ConsoleSessionLauncher(const std::wstring& service_id);
    ~ConsoleSessionLauncher() = default;

    void ExecuteService(uint32_t session_id, const std::wstring& channel_id);

private:
    void Worker() override;
    void OnStop() override;

private:
    static const uint32_t kInvalidSessionId = 0xFFFFFFFF;

    uint32_t session_id_ = kInvalidSessionId;
    std::wstring channel_id_;

    DISALLOW_COPY_AND_ASSIGN(ConsoleSessionLauncher);
};

bool LaunchDesktopSession(uint32_t session_id, const std::wstring& channel_id);

bool LaunchFileTransferSession(uint32_t session_id, const std::wstring& channel_id);

} // namespace aspia

#endif // _ASPIA_HOST__CONSOLE_SESSION_LAUNCHER_H
