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

#include "base/win/session_info.h"

#include "base/logging.h"
#include "base/win/windows_version.h"

namespace base {

//--------------------------------------------------------------------------------------------------
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
        PLOG(ERROR) << "WTSQuerySessionInformationW failed";
        return;
    }

    info_.reset(info.release());
}

//--------------------------------------------------------------------------------------------------
SessionInfo::~SessionInfo() = default;

//--------------------------------------------------------------------------------------------------
bool SessionInfo::isValid() const
{
    return info_.isValid();
}

//--------------------------------------------------------------------------------------------------
SessionId SessionInfo::sessionId() const
{
    if (!isValid())
        return kInvalidSessionId;

    return info_->Data.WTSInfoExLevel1.SessionId;
}

//--------------------------------------------------------------------------------------------------
SessionInfo::ConnectState SessionInfo::connectState() const
{
    if (!isValid())
        return ConnectState::DISCONNECTED;

    return static_cast<ConnectState>(info_->Data.WTSInfoExLevel1.SessionState);
}

//--------------------------------------------------------------------------------------------------
QString SessionInfo::winStationName() const
{
    if (!isValid())
        return QString();

    return QString::fromWCharArray(info_->Data.WTSInfoExLevel1.WinStationName);
}

//--------------------------------------------------------------------------------------------------
QString SessionInfo::domain() const
{
    if (!isValid())
        return QString();

    return QString::fromWCharArray(info_->Data.WTSInfoExLevel1.DomainName);
}

//--------------------------------------------------------------------------------------------------
QString SessionInfo::userName() const
{
    if (!isValid())
        return QString();

    return QString::fromWCharArray(info_->Data.WTSInfoExLevel1.UserName);
}

//--------------------------------------------------------------------------------------------------
QString SessionInfo::clientName() const
{
    if (!isValid())
        return QString();

    LPWSTR client_name = nullptr;
    DWORD size = 0;

    if (!WTSQuerySessionInformationW(
        WTS_CURRENT_SERVER_HANDLE, sessionId(), WTSClientName, &client_name, &size))
    {
        LOG(ERROR) << "WTSQuerySessionInformationW() failed:" << ::GetLastError();
        return QString();
    }

    if (!client_name)
    {
        LOG(ERROR) << "Invalid client name";
        return QString();
    }

    QString result = QString::fromWCharArray(client_name);
    WTSFreeMemory(client_name);
    return result;
}

//--------------------------------------------------------------------------------------------------
qint64 SessionInfo::connectTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.ConnectTime.QuadPart;
}

//--------------------------------------------------------------------------------------------------
qint64 SessionInfo::disconnectTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.DisconnectTime.QuadPart;
}

//--------------------------------------------------------------------------------------------------
qint64 SessionInfo::lastInputTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.LastInputTime.QuadPart;
}

//--------------------------------------------------------------------------------------------------
qint64 SessionInfo::logonTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.LogonTime.QuadPart;
}

//--------------------------------------------------------------------------------------------------
qint64 SessionInfo::currentTime() const
{
    if (!isValid())
        return 0;

    return info_->Data.WTSInfoExLevel1.CurrentTime.QuadPart;
}

//--------------------------------------------------------------------------------------------------
bool SessionInfo::isUserLocked() const
{
    if (!isValid())
        return false;

    if (base::OSInfo::instance()->version() <= VERSION_WIN7)
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

} // namespace base
