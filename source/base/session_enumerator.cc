//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/session_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/session_enumerator.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

SessionEnumerator::SessionEnumerator()
{
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, session_info_.Recieve(), &count_))
    {
        DLOG(WARNING) << "WTSEnumerateSessionsW() failed: " << GetLastSystemErrorString();
        session_info_.Set(nullptr);
        count_ = 0;
    }
}

bool SessionEnumerator::IsAtEnd() const
{
    if (!session_info_.IsValid())
        return true;

    return current_ >= count_;
}

void SessionEnumerator::Advance()
{
    ++current_;
}

std::string SessionEnumerator::GetUserName() const
{
    ScopedWtsMemory<WCHAR*> buffer;
    DWORD bytes_returned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_info_[current_]->SessionId,
                                     WTSUserName,
                                     buffer.Recieve(),
                                     &bytes_returned))
    {
        DLOG(WARNING) << "WTSQuerySessionInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(buffer);
}

std::string SessionEnumerator::GetDomainName() const
{
    ScopedWtsMemory<WCHAR*> buffer;
    DWORD bytes_returned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_info_[current_]->SessionId,
                                     WTSDomainName,
                                     buffer.Recieve(),
                                     &bytes_returned))
    {
        DLOG(WARNING) << "WTSQuerySessionInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(buffer);
}

uint32_t SessionEnumerator::GetSessionId() const
{
    return session_info_[current_]->SessionId;
}

std::string SessionEnumerator::GetConnectState() const
{
    ScopedWtsMemory<int*> buffer;
    DWORD bytes_returned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_info_[current_]->SessionId,
                                     WTSConnectState,
                                     reinterpret_cast<LPWSTR*>(buffer.Recieve()),
                                     &bytes_returned))
    {
        DLOG(WARNING) << "WTSQuerySessionInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    switch (*buffer)
    {
        case WTSActive:
            return "Active";

        case WTSConnected:
            return "Connected";

        case WTSConnectQuery:
            return "Connect Query";

        case WTSShadow:
            return "Shadow";

        case WTSDisconnected:
            return "Disconnected";

        case WTSIdle:
            return "Idle";

        case WTSListen:
            return "Listen";

        case WTSReset:
            return "Reset";

        case WTSDown:
            return "Down";

        case WTSInit:
            return "Init";

        default:
            return std::string();
    }
}

std::string SessionEnumerator::GetClientName() const
{
    ScopedWtsMemory<WCHAR*> buffer;
    DWORD bytes_returned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_info_[current_]->SessionId,
                                     WTSClientName,
                                     buffer.Recieve(),
                                     &bytes_returned))
    {
        DLOG(WARNING) << "WTSQuerySessionInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(buffer);
}

std::string SessionEnumerator::GetWinStationName() const
{
    ScopedWtsMemory<WCHAR*> buffer;
    DWORD bytes_returned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     session_info_[current_]->SessionId,
                                     WTSWinStationName,
                                     buffer.Recieve(),
                                     &bytes_returned))
    {
        DLOG(WARNING) << "WTSQuerySessionInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(buffer);
}

} // namespace aspia
