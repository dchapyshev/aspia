//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace base::win {

SessionInfo::SessionInfo(SessionId session_id)
{
    ScopedWtsMemory<WTSINFOW> info;
    DWORD bytes_returned;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_id,
                                     WTSSessionInfo,
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

    return info_->SessionId;
}

SessionInfo::ConnectState SessionInfo::connectState() const
{
    if (!isValid())
        return ConnectState::DISCONNECTED;

    return static_cast<ConnectState>(info_->State);
}

std::string SessionInfo::winStationName() const
{
    if (!isValid())
        return std::string();

    return utf8FromWide(info_->WinStationName);
}

std::string SessionInfo::domain() const
{
    if (!isValid())
        return std::string();

    return utf8FromWide(info_->Domain);
}

std::string SessionInfo::userName() const
{
    if (!isValid())
        return std::string();

    return utf8FromWide(info_->UserName);
}

int64_t SessionInfo::connectTime() const
{
    if (!isValid())
        return 0;

    return info_->ConnectTime.QuadPart;
}

int64_t SessionInfo::disconnectTime() const
{
    if (!isValid())
        return 0;

    return info_->DisconnectTime.QuadPart;
}

int64_t SessionInfo::lastInputTime() const
{
    if (!isValid())
        return 0;

    return info_->LastInputTime.QuadPart;
}

int64_t SessionInfo::logonTime() const
{
    if (!isValid())
        return 0;

    return info_->LogonTime.QuadPart;
}

int64_t SessionInfo::currentTime() const
{
    if (!isValid())
        return 0;

    return info_->CurrentTime.QuadPart;
}

} // namespace base::win
