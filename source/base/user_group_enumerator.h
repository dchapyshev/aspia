//
// PROJECT:         Aspia
// FILE:            base/user_group_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__USER_GROUP_ENUMERATOR_H
#define _ASPIA_BASE__USER_GROUP_ENUMERATOR_H

#include "base/macros.h"

#include <lm.h>
#include <string>

namespace aspia {

class UserGroupEnumerator
{
public:
    UserGroupEnumerator();
    ~UserGroupEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetName() const;
    std::string GetComment() const;

private:
    PLOCALGROUP_INFO_1 group_info_ = nullptr;
    DWORD total_entries_ = 0;
    DWORD current_entry_ = 0;

    DISALLOW_COPY_AND_ASSIGN(UserGroupEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__USER_GROUP_ENUMERATOR_H
