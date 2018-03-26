//
// PROJECT:         Aspia
// FILE:            base/win/scoped_privilege.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_PRIVILEGE_H
#define _ASPIA_BASE__WIN__SCOPED_PRIVILEGE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "base/win/scoped_object.h"

namespace aspia {

class ScopedProcessPrivilege
{
public:
    explicit ScopedProcessPrivilege(const wchar_t* name);
    ~ScopedProcessPrivilege();

    bool IsSuccessed() const { return is_enabled_; }

private:
    bool is_enabled_ = false;
    ScopedHandle process_token_;
    TOKEN_PRIVILEGES privileges_;

    DISALLOW_COPY_AND_ASSIGN(ScopedProcessPrivilege);
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_PRIVILEGE_H
