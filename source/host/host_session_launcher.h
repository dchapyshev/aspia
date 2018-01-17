//
// PROJECT:         Aspia
// FILE:            host/host_session_launcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
#define _ASPIA_HOST__HOST_SESSION_LAUNCHER_H

#include "proto/auth_session.pb.h"

#include <cstdint>

namespace aspia {

extern const WCHAR kSessionLauncherSwitch[];
extern const WCHAR kDesktopSessionSwitch[];
extern const WCHAR kFileTransferSessionSwitch[];
extern const WCHAR kPowerManageSessionSwitch[];
extern const WCHAR kSystemInfoSessionSwitch[];
extern const WCHAR kSystemInfoSwitch[];

bool LaunchSessionProcessFromService(const std::wstring& run_mode,
                                     uint32_t session_id,
                                     const std::wstring& channel_id);

bool LaunchSessionProcess(proto::auth::SessionType session_type,
                          uint32_t session_id,
                          const std::wstring& channel_id);

bool LaunchSystemInfoProcess();

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
