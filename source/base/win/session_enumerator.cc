//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/session_enumerator.h"

#include "base/logging.h"
#include "base/win/session_info.h"

namespace base {

//--------------------------------------------------------------------------------------------------
SessionEnumerator::SessionEnumerator()
{
    DWORD level = 1;

    if (!WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level, 0, info_.receive(), &count_))
    {
        PLOG(ERROR) << "WTSEnumerateSessionsExW failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
SessionEnumerator::~SessionEnumerator() = default;

//--------------------------------------------------------------------------------------------------
void SessionEnumerator::advance()
{
    ++current_;
}

//--------------------------------------------------------------------------------------------------
bool SessionEnumerator::isAtEnd() const
{
    return current_ >= count_;
}

//--------------------------------------------------------------------------------------------------
// static
const char* SessionEnumerator::stateToString(WTS_CONNECTSTATE_CLASS state)
{
    switch (state)
    {
        case WTSActive:
            return "WTSActive";
        case WTSConnected:
            return "WTSConnected";
        case WTSConnectQuery:
            return "WTSConnectQuery";
        case WTSShadow:
            return "WTSShadow";
        case WTSDisconnected:
            return "WTSDisconnected";
        case WTSIdle:
            return "WTSIdle";
        case WTSListen:
            return "WTSListen";
        case WTSReset:
            return "WTSReset";
        case WTSDown:
            return "WTSDown";
        case WTSInit:
            return "WTSInit";
        default:
            return "Unknown";
    }
}

//--------------------------------------------------------------------------------------------------
WTS_CONNECTSTATE_CLASS SessionEnumerator::state() const
{
    return info_[current_]->State;
}

//--------------------------------------------------------------------------------------------------
SessionId SessionEnumerator::sessionId() const
{
    return info_[current_]->SessionId;
}

//--------------------------------------------------------------------------------------------------
QString SessionEnumerator::sessionName() const
{
    if (!info_[current_]->pSessionName)
        return QString();

    return QString::fromWCharArray(info_[current_]->pSessionName);
}

//--------------------------------------------------------------------------------------------------
QString SessionEnumerator::hostName() const
{
    if (!info_[current_]->pHostName)
        return QString();

    return QString::fromWCharArray(info_[current_]->pHostName);
}

//--------------------------------------------------------------------------------------------------
QString SessionEnumerator::userName() const
{
    if (!info_[current_]->pUserName)
        return QString();

    return QString::fromWCharArray(info_[current_]->pUserName);
}

//--------------------------------------------------------------------------------------------------
QString SessionEnumerator::domainName() const
{
    if (!info_[current_]->pDomainName)
        return QString();

    return QString::fromWCharArray(info_[current_]->pDomainName);
}

//--------------------------------------------------------------------------------------------------
QString SessionEnumerator::farmName() const
{
    if (!info_[current_]->pFarmName)
        return QString();

    return QString::fromWCharArray(info_[current_]->pFarmName);
}

//--------------------------------------------------------------------------------------------------
bool SessionEnumerator::isConsole() const
{
    if (!info_[current_]->pSessionName)
        return false;

    return _wcsicmp(info_[current_]->pSessionName, L"console") == 0;
}

//--------------------------------------------------------------------------------------------------
bool SessionEnumerator::isActive() const
{
    return state() == WTSActive;
}

//--------------------------------------------------------------------------------------------------
bool SessionEnumerator::isUserLocked() const
{
    SessionInfo session_info(sessionId());
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info";
        return false;
    }

    return session_info.isUserLocked();
}

} // namespace base
