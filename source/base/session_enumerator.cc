//
// PROJECT:         Aspia
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

    while (true)
    {
        if (current_ >= count_)
            return true;

        // We skip the system account.
        if (session_info_[current_]->SessionId == 0)
        {
            ++current_;
        }
        else
        {
            return false;
        }
    }
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

proto::Sessions::ConnectState SessionEnumerator::GetConnectState() const
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
        return proto::Sessions::CONNECT_STATE_UNKNOWN;
    }

    switch (*buffer)
    {
        case WTSActive: return proto::Sessions::CONNECT_STATE_ACTIVE;
        case WTSConnected: return proto::Sessions::CONNECT_STATE_CONNECTED;
        case WTSConnectQuery: return proto::Sessions::CONNECT_STATE_CONNECT_QUERY;
        case WTSShadow: return proto::Sessions::CONNECT_STATE_SHADOW;
        case WTSDisconnected: return proto::Sessions::CONNECT_STATE_DISCONNECTED;
        case WTSIdle: return proto::Sessions::CONNECT_STATE_IDLE;
        case WTSListen: return proto::Sessions::CONNECT_STATE_LISTEN;
        case WTSReset: return proto::Sessions::CONNECT_STATE_RESET;
        case WTSDown: return proto::Sessions::CONNECT_STATE_DOWN;
        case WTSInit: return proto::Sessions::CONNECT_STATE_INIT;
        default: return proto::Sessions::CONNECT_STATE_UNKNOWN;
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
