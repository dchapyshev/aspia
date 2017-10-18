//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/user_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/user_enumerator.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

UserEnumerator::UserEnumerator()
{
    DWORD entries_read = 0;

    DWORD error_code = NetUserEnum(nullptr, 3,
                                   FILTER_NORMAL_ACCOUNT,
                                    reinterpret_cast<LPBYTE*>(&user_info_),
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

UserEnumerator::~UserEnumerator()
{
    if (user_info_)
        NetApiBufferFree(user_info_);
}

bool UserEnumerator::IsAtEnd() const
{
    if (!user_info_)
        return true;

    return current_entry_ >= total_entries_;
}

void UserEnumerator::Advance()
{
    ++current_entry_;
}

std::string UserEnumerator::GetName() const
{
    return UTF8fromUNICODE(user_info_[current_entry_].usri3_name);
}

std::string UserEnumerator::GetFullName() const
{
    return UTF8fromUNICODE(user_info_[current_entry_].usri3_full_name);
}

std::string UserEnumerator::GetComment() const
{
    return UTF8fromUNICODE(user_info_[current_entry_].usri3_comment);
}

bool UserEnumerator::IsDisabled() const
{
    return (user_info_[current_entry_].usri3_flags & UF_ACCOUNTDISABLE);
}

bool UserEnumerator::IsPasswordCantChange() const
{
    return (user_info_[current_entry_].usri3_flags & UF_PASSWD_CANT_CHANGE);
}

bool UserEnumerator::IsPasswordExpired() const
{
    return (user_info_[current_entry_].usri3_flags & UF_PASSWORD_EXPIRED);
}

bool UserEnumerator::IsDontExpirePassword() const
{
    return (user_info_[current_entry_].usri3_flags & UF_DONT_EXPIRE_PASSWD);
}

bool UserEnumerator::IsLockout() const
{
    return (user_info_[current_entry_].usri3_flags & UF_LOCKOUT);
}

uint32_t UserEnumerator::GetNumberLogons() const
{
    return user_info_[current_entry_].usri3_num_logons;
}

uint32_t UserEnumerator::GetBadPasswordCount() const
{
    return user_info_[current_entry_].usri3_bad_pw_count;
}

time_t UserEnumerator::GetLastLogonTime() const
{
    return user_info_[current_entry_].usri3_last_logon;
}

} // namespace aspia
