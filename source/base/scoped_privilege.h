//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_privilege.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_PRIVILEGE_H
#define _ASPIA_BASE__SCOPED_PRIVILEGE_H

#include "base/scoped_object.h"

namespace aspia {

class ScopedProcessPrivilege
{
public:
    ScopedProcessPrivilege(const WCHAR* name);
    ~ScopedProcessPrivilege();

    bool IsSuccessed() const { return is_enabled_; }

private:
    bool is_enabled_ = false;
    ScopedHandle process_token_;
    TOKEN_PRIVILEGES privileges_;

    DISALLOW_COPY_AND_ASSIGN(ScopedProcessPrivilege);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_PRIVILEGE_H
