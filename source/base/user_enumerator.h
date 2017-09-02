//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/user_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__USER_ENUMERATOR_H
#define _ASPIA_BASE__USER_ENUMERATOR_H

#include "base/macros.h"

#include <lm.h>
#include <string>

namespace aspia {

class UserEnumerator
{
public:
    UserEnumerator();
    ~UserEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetFullName() const;
    std::string GetComment() const;
    bool IsDisabled() const;
    bool IsPasswordCantChange() const;
    bool IsPasswordExpired() const;
    bool IsDontExpirePassword() const;
    bool IsLockout() const;
    uint32_t GetNumberLogons() const;
    uint32_t GetBadPasswordCount() const;
    time_t GetLastLogonTime() const;
    uint32_t GetUserId() const;
    std::string GetProfileDirectory() const;
    std::string GetHomeDirectory() const;
    std::string GetScriptPath() const;
    uint32_t GetCountryCode() const;
    uint32_t GetCodePage() const;
    uint32_t GetPrimaryGroupId() const;

private:
    PUSER_INFO_3 user_info_ = nullptr;
    DWORD total_entries_ = 0;
    DWORD current_entry_ = 0;

    DISALLOW_COPY_AND_ASSIGN(UserEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__USER_ENUMERATOR_H
