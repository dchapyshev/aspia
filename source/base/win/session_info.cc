//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/win/session_info.h"

#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/win/windows_version.h"

namespace base::win {

SessionInfo::SessionInfo(SessionId session_id)
{
    ScopedWtsMemory<WTSINFOEXW> info;
    DWORD bytes_returned;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_id,
                                     WTSSessionInfoEx,
                                     reinterpret_cast<LPWSTR*>(info.recieve()),
                                     &bytes_returned))
    {
        PLOG(LS_WARNING) << "WTSQuerySessionInformationW failed";
        return;
    }

    info_.reset(info.release());
}

SessionInfo::~SessionInfo() = default;

bool SessionInfo::isValid() const
{
    return info_.isValid();
}

SessionId SessionInfo::sessionId() const
{
    if (!isValid())
        return kInvalidSessionId;

    return info_->Data.WTSInfoExLevel1.SessionId;
}

SessionInfo::ConnectState SessionInfo::connectState() const
{
    if (!isValid())
        return ConnectState::DISCONNECTED;

    return static_cast<ConnectState>(info_->Data.WTSInfoExLevel1.SessionState);
}

// static
const char* SessionInfo::connectStateToString(ConnectState connect_state)
{
    switch (connect_state)
    {
        case ConnectState::ACTIVE:
            return "ACTIVE";
        case ConnectState::CONNECTED:
            return "CONNECTED";
        case ConnectState::CONNECT_QUERY:
            return "CONNECT_QUERY";
        case ConnectState::SHADOW:
            return "SHADOW";
        case ConnectState::DISCONNECTED:
            return "DISCONNECTED";
        case ConnectState::IDLE:
            return "IDLE";
        case ConnectState::LISTEN:
            return "LISTEN";
        case ConnectState::RESET:
            return "RESET";
        case ConnectState::DOWN:
            return "DOWN";
        case ConnectState::INIT:
            return "INIT";
        default:
            return "UNKNOWN";
    }
}

std::string SessionInfo::winStationName() const
{
    return utf8FromUtf16(winStationName16());
}

std::u16string SessionInfo::winStationName16() const
{
    if (!isValid())
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_->Data.WTSInfoExLevel1.WinStationName);
}

std::string SessionInfo::domain() const
{
    return utf8FromUtf16(domain16());
}

std::u16string SessionInfo::domain16() const
{
    if (!isValid())
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_->Data.WTSInfoExLevel1.DomainName);
}

std::string SessionInfo::userName() const
{
    return utf8FromUtf16(userName16());
}

std::u16string SessionInfo::userName16() const
{
    if (!isValid())
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_->Data.WTSInfoExLevel1.UserName);
}

std::string SessionInfo::clientName() const
{
    return utf8FromUtf16(clientName16());
}

std::u16string SessionInfo::clientName16() const
{
    if (!isValid())
        return std::u16string();

    LPWSTR client_name = nullptr;
    DWORD size = 0;

    if (!WTSQuerySessionInformationW(
        WTS_CURRENT_SERVER_HANDLE, sessionId(), WTSClientName, &client_name, &size))
    {
        LOG(LS_WARNING) << "WTSQuerySessionInformationW() failed: " << ::GetLastError();
        return std::u16string();
    }

    if (!client_name)
    {
        LOG(LS_WARNING) << "Invalid client name";
        return std::u16string();
    }

    std::u16string result;
    result.assign(reinterpret_cast<const char16_t*>(client_name));

    WTSFreeMemory(client_name);
    return result;
}

int64_t SessionInfo::connectTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.ConnectTime.QuadPart;
}

int64_t SessionInfo::disconnectTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.DisconnectTime.QuadPart;
}

int64_t SessionInfo::lastInputTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.LastInputTime.QuadPart;
}

int64_t SessionInfo::logonTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.LogonTime.QuadPart;
}

int64_t SessionInfo::currentTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.CurrentTime.QuadPart;
}

bool SessionInfo::isUserLocked() const
{
    if (!isValid())
        return false;

    if (base::win::OSInfo::instance()->version() <= VERSION_WIN7)
    {
        // Due to a code defect, the usage of the WTS_SESSIONSTATE_LOCK and WTS_SESSIONSTATE_UNLOCK
        // flags is reversed. That is, WTS_SESSIONSTATE_LOCK indicates that the session is unlocked,
        // and WTS_SESSIONSTATE_UNLOCK indicates the session is locked.
        return info_->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    else
    {
        return info_->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
    }
}

} // namespace base::win
