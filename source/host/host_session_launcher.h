//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_launcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
#define _ASPIA_HOST__HOST_SESSION_LAUNCHER_H

#include "proto/auth_session.pb.h"

#include <cstdint>

namespace aspia {

static const WCHAR kSessionLauncherSwitch[] = L"session-launcher";
static const WCHAR kDesktopSessionSwitch[] = L"desktop-session";
static const WCHAR kFileTransferSessionSwitch[] = L"file-transfer-session";
static const WCHAR kPowerManageSessionSwitch[] = L"power-manage-session";
static const WCHAR kSystemInfoSessionSwitch[] = L"system-info-session";

bool LaunchSessionProcessFromService(const std::wstring& run_mode,
                                     uint32_t session_id,
                                     const std::wstring& channel_id);

bool LaunchSessionProcess(proto::SessionType session_type,
                          uint32_t session_id,
                          const std::wstring& channel_id);

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
