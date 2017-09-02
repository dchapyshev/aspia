//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/user_group_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/user_group_enumerator.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

UserGroupEnumerator::UserGroupEnumerator()
{
    DWORD entries_read = 0;

    DWORD error_code = NetLocalGroupEnum(nullptr, 1,
                                         reinterpret_cast<LPBYTE*>(&group_info_),
                                         MAX_PREFERRED_LENGTH,
                                         &entries_read,
                                         &total_entries_,
                                         nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(WARNING) << "NetUserEnum() failed: " << SystemErrorCodeToString(error_code);
        return;
    }

    DCHECK(entries_read == total_entries_);
}

UserGroupEnumerator::~UserGroupEnumerator()
{
    if (group_info_)
        NetApiBufferFree(group_info_);
}

bool UserGroupEnumerator::IsAtEnd() const
{
    if (!group_info_)
        return true;

    return current_entry_ >= total_entries_;
}

void UserGroupEnumerator::Advance()
{
    ++current_entry_;
}

std::string UserGroupEnumerator::GetName() const
{
    return UTF8fromUNICODE(group_info_[current_entry_].lgrpi1_name);
}

std::string UserGroupEnumerator::GetComment() const
{
    return UTF8fromUNICODE(group_info_[current_entry_].lgrpi1_comment);
}

} // namespace aspia
