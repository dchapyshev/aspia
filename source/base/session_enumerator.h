//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/session_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SESSION_ENUMERATOR_H
#define _ASPIA_BASE__SESSION_ENUMERATOR_H

#include "base/scoped_wts_memory.h"
#include "base/macros.h"

#include <string>

namespace aspia {

class SessionEnumerator
{
public:
    SessionEnumerator();
    ~SessionEnumerator() = default;

    bool IsAtEnd() const;
    void Advance();

    std::string GetUserName() const;
    std::string GetDomainName() const;
    uint32_t GetSessionId() const;
    std::string GetConnectState() const;
    std::string GetClientName() const;
    std::string GetWinStationName() const;

private:
    ScopedWtsMemory<PWTS_SESSION_INFOW> session_info_;
    DWORD count_ = 0;
    mutable DWORD current_ = 0;

    DISALLOW_COPY_AND_ASSIGN(SessionEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__SESSION_ENUMERATOR_H
