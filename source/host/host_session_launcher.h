//
// PROJECT:         Aspia
// FILE:            host/host_session_launcher.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
#define _ASPIA_HOST__HOST_SESSION_LAUNCHER_H

#include "protocol/authorization.pb.h"

#include <cstdint>

namespace aspia {

bool LaunchSessionProcess(proto::auth::SessionType session_type,
                          uint32_t session_id,
                          const std::wstring& channel_id);

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_LAUNCHER_H
