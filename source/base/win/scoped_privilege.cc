//
// PROJECT:         Aspia
// FILE:            base/win/scoped_privilege.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/win/scoped_privilege.h"

#include <QDebug>

#include "base/system_error_code.h"

namespace aspia {

ScopedProcessPrivilege::ScopedProcessPrivilege(const wchar_t* name)
{
    Q_ASSERT(name);

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES,
                          process_token_.Recieve()))
    {
        qWarning() << "OpenProcessToken failed: " << lastSystemErrorString();
        return;
    }

    memset(&privileges_, 0, sizeof(privileges_));

    if (!LookupPrivilegeValueW(nullptr,
                               name,
                               &privileges_.Privileges[0].Luid))
    {
        qWarning() << "LookupPrivilegeValueW failed: " << lastSystemErrorString();
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
        qWarning() << "AdjustTokenPrivileges failed: " << lastSystemErrorString();
        return;
    }

        is_enabled_ = true;
}

ScopedProcessPrivilege::~ScopedProcessPrivilege()
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
            qWarning() << "AdjustTokenPrivileges failed: " << lastSystemErrorString();
        }
    }
}

} // namespace aspia
