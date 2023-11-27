//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/strings/unicode.h"
#include "base/win/session_info.h"

namespace base::win {

//--------------------------------------------------------------------------------------------------
SessionEnumerator::SessionEnumerator()
{
    DWORD level = 1;

    if (!WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level, 0, info_.recieve(), &count_))
    {
        PLOG(LS_ERROR) << "WTSEnumerateSessionsExW failed";
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
std::string SessionEnumerator::sessionName() const
{
    return base::utf8FromUtf16(sessionName16());
}

//--------------------------------------------------------------------------------------------------
std::u16string SessionEnumerator::sessionName16() const
{
    if (!info_[current_]->pSessionName)
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_[current_]->pSessionName);
}

//--------------------------------------------------------------------------------------------------
std::string SessionEnumerator::hostName() const
{
    return base::utf8FromUtf16(hostName16());
}

//--------------------------------------------------------------------------------------------------
std::u16string SessionEnumerator::hostName16() const
{
    if (!info_[current_]->pHostName)
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_[current_]->pHostName);
}

//--------------------------------------------------------------------------------------------------
std::string SessionEnumerator::userName() const
{
    return base::utf8FromUtf16(userName16());
}

//--------------------------------------------------------------------------------------------------
std::u16string SessionEnumerator::userName16() const
{
    if (!info_[current_]->pUserName)
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_[current_]->pUserName);
}

//--------------------------------------------------------------------------------------------------
std::string SessionEnumerator::domainName() const
{
    return base::utf8FromUtf16(domainName16());
}

//--------------------------------------------------------------------------------------------------
std::u16string SessionEnumerator::domainName16() const
{
    if (!info_[current_]->pDomainName)
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_[current_]->pDomainName);
}

//--------------------------------------------------------------------------------------------------
std::string SessionEnumerator::farmName() const
{
    return base::utf8FromUtf16(farmName16());
}

//--------------------------------------------------------------------------------------------------
std::u16string SessionEnumerator::farmName16() const
{
    if (!info_[current_]->pFarmName)
        return std::u16string();

    return reinterpret_cast<const char16_t*>(info_[current_]->pFarmName);
}

//--------------------------------------------------------------------------------------------------
bool SessionEnumerator::isUserLocked() const
{
    SessionInfo session_info(sessionId());
    if (!session_info.isValid())
    {
        LOG(LS_ERROR) << "Unable to get session info";
        return false;
    }

    return session_info.isUserLocked();
}

} // namespace base::win
