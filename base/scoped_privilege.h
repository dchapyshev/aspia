//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_privilege.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_PRIVILEGE_H
#define _ASPIA_BASE__SCOPED_PRIVILEGE_H

#include "base/scoped_object.h"
#include "base/logging.h"

namespace aspia {

class ScopedProcessPrivilege
{
public:
    ScopedProcessPrivilege(const WCHAR* name)
    {
        DCHECK(name);

        if (!OpenProcessToken(GetCurrentProcess(),
                              TOKEN_ADJUST_PRIVILEGES,
                              process_token_.Recieve()))
        {
            LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
            return;
        }

        memset(&privileges_, 0, sizeof(privileges_));

        if (!LookupPrivilegeValueW(nullptr,
                                   name,
                                   &privileges_.Privileges[0].Luid))
        {
            LOG(ERROR) << "LookupPrivilegeValueW() failed: " << GetLastError();
            return;
        }

        privileges_.PrivilegeCount = 1;
        privileges_.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(process_token_,
                                   FALSE,
                                   &privileges_,
                                   0,
                                   nullptr,
                                   nullptr))
        {
            LOG(ERROR) << "AdjustTokenPrivileges() failed: " << GetLastError();
            return;
        }

        is_enabled_ = true;
    }

    ~ScopedProcessPrivilege()
    {
        if (is_enabled_)
        {
            privileges_.PrivilegeCount = 1;
            privileges_.Privileges[0].Attributes = 0;

            if (!AdjustTokenPrivileges(process_token_,
                                       FALSE,
                                       &privileges_,
                                       0,
                                       nullptr,
                                       nullptr))
            {
                LOG(ERROR) << "AdjustTokenPrivileges() failed: " << GetLastError();
            }
        }
    }

    bool IsSuccessed() const { return is_enabled_; }

private:
    bool is_enabled_ = false;
    ScopedHandle process_token_;
    TOKEN_PRIVILEGES privileges_;

    DISALLOW_COPY_AND_ASSIGN(ScopedProcessPrivilege);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_PRIVILEGE_H
