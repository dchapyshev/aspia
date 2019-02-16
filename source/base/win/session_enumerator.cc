//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/unicode.h"

namespace base::win {

SessionEnumerator::SessionEnumerator()
{
    DWORD level = 1;

    if (!WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level, 0, info_.recieve(), &count_))
    {
        PLOG(LS_WARNING) << "WTSEnumerateSessionsExW failed";
        return;
    }
}

SessionEnumerator::~SessionEnumerator() = default;

void SessionEnumerator::advance()
{
    ++current_;
}

bool SessionEnumerator::isAtEnd() const
{
    return current_ >= count_;
}

SessionEnumerator::State SessionEnumerator::state() const
{
    switch (info_[current_]->State)
    {
        case WTSActive:
            return State::ACTIVE;

        case WTSConnected:
            return State::CONNECTED;

        case WTSConnectQuery:
            return State::CONNECT_QUERY;

        case WTSShadow:
            return State::SHADOW;

        case WTSDisconnected:
            return State::DISCONNECTED;

        case WTSIdle:
            return State::IDLE;

        case WTSListen:
            return State::LISTEN;

        case WTSReset:
            return State::RESET;

        case WTSDown:
            return State::DOWN;

        case WTSInit:
            return State::INIT;

        default:
            return State::UNKNOWN;
    }
}

uint32_t SessionEnumerator::sessionId() const
{
    return info_[current_]->SessionId;
}

std::string SessionEnumerator::sessionName() const
{
    return base::UTF8fromUTF16(info_[current_]->pSessionName);
}

std::string SessionEnumerator::hostName() const
{
    return base::UTF8fromUTF16(info_[current_]->pHostName);
}

std::string SessionEnumerator::userName() const
{
    return base::UTF8fromUTF16(info_[current_]->pUserName);
}

std::string SessionEnumerator::domainName() const
{
    return base::UTF8fromUTF16(info_[current_]->pDomainName);
}

std::string SessionEnumerator::farmName() const
{
    return base::UTF8fromUTF16(info_[current_]->pFarmName);
}

} // namespace base::win
