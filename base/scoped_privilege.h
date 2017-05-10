//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_privilege.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_PRIVILEGE_H
#define _ASPIA_BASE__SCOPED_PRIVILEGE_H

#include <string>

#include "base/scoped_object.h"
#include "base/logging.h"

namespace aspia {

class ScopedProcessPrivilege
{
public:
    ScopedProcessPrivilege() = default;

    ~ScopedProcessPrivilege()
    {
        EnablePrivilege(false);
    }

    bool Enable(const std::wstring& name)
    {
        name_ = name;

        if (!OpenProcessToken(GetCurrentProcess(),
                              TOKEN_ADJUST_PRIVILEGES,
                              process_token_.Recieve()))
        {
            LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
            return false;
        }

        return EnablePrivilege(true);
    }

private:
    bool EnablePrivilege(bool enable)
    {
        if (!process_token_.IsValid())
            return false;

        TOKEN_PRIVILEGES privileges;

        if (!LookupPrivilegeValueW(nullptr,
                                   name_.c_str(),
                                   &privileges.Privileges[0].Luid))
        {
            LOG(ERROR) << "LookupPrivilegeValueW() failed: " << GetLastError();
            return false;
        }

        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

        if (!AdjustTokenPrivileges(process_token_,
                                   FALSE,
                                   &privileges,
                                   0,
                                   nullptr,
                                   nullptr))
        {
            LOG(ERROR) << "AdjustTokenPrivileges() failed: " << GetLastError();
            return false;
        }

        return true;
    }

private:
    ScopedHandle process_token_;
    std::wstring name_;

    DISALLOW_COPY_AND_ASSIGN(ScopedProcessPrivilege);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_PRIVILEGE_H
