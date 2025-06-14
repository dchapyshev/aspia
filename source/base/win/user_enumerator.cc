//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/win/user_enumerator.h"

#include "base/logging.h"
#include "base/system_error.h"
#include "base/win/user_group_enumerator.h"

#include <qt_windows.h>
#include <lm.h>

namespace base {

//--------------------------------------------------------------------------------------------------
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
        LOG(ERROR) << "NetUserEnum failed:" << SystemError(error_code).toString();
        return;
    }
}

//--------------------------------------------------------------------------------------------------
UserEnumerator::~UserEnumerator()
{
    if (user_info_)
        NetApiBufferFree(user_info_);
}

//--------------------------------------------------------------------------------------------------
void UserEnumerator::advance()
{
    ++current_entry_;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isAtEnd() const
{
    return current_entry_ >= total_entries_;
}

//--------------------------------------------------------------------------------------------------
QString UserEnumerator::name() const
{
    if (!user_info_[current_entry_].usri3_name)
        return QString();
    return QString::fromWCharArray(user_info_[current_entry_].usri3_name);
}

//--------------------------------------------------------------------------------------------------
QString UserEnumerator::fullName() const
{
    if (!user_info_[current_entry_].usri3_full_name)
        return QString();
    return QString::fromWCharArray(user_info_[current_entry_].usri3_full_name);
}

//--------------------------------------------------------------------------------------------------
QString UserEnumerator::comment() const
{
    if (!user_info_[current_entry_].usri3_comment)
        return QString();
    return QString::fromWCharArray(user_info_[current_entry_].usri3_comment);
}

//--------------------------------------------------------------------------------------------------
QString UserEnumerator::homeDir() const
{
    if (!user_info_[current_entry_].usri3_home_dir)
        return QString();
    return QString::fromWCharArray(user_info_[current_entry_].usri3_home_dir);
}

//--------------------------------------------------------------------------------------------------
QVector<std::pair<QString, QString>> UserEnumerator::groups() const
{
    const wchar_t* user_name = user_info_[current_entry_].usri3_name;
    if (!user_name)
        return {};

    LPLOCALGROUP_USERS_INFO_0 group_info = nullptr;
    DWORD total_entries = 0;
    DWORD entries_read = 0;

    DWORD error_code = NetUserGetLocalGroups(nullptr, // server
                                             user_name,
                                             0, // level
                                             LG_INCLUDE_INDIRECT,
                                             reinterpret_cast<LPBYTE*>(&group_info),
                                             MAX_PREFERRED_LENGTH,
                                             &entries_read,
                                             &total_entries);
    if (error_code != NERR_Success)
    {
        LOG(ERROR) << "NetUserGetLocalGroups failed:" << SystemError(error_code).toString();
        return {};
    }

    QVector<std::pair<QString, QString>> result;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        if (group_info[i].lgrui0_name)
        {
            QString name = QString::fromWCharArray(group_info[i].lgrui0_name);
            QString comment;

            for (UserGroupEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
            {
                if (enumerator.name() == name)
                {
                    comment = enumerator.comment();
                    break;
                }
            }

            result.append(std::make_pair(name, comment));
        }
    }

    NetApiBufferFree(group_info);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isDisabled() const
{
    return user_info_[current_entry_].usri3_flags & UF_ACCOUNTDISABLE;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isPasswordCantChange() const
{
    return user_info_[current_entry_].usri3_flags & UF_PASSWD_CANT_CHANGE;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isPasswordExpired() const
{
    return user_info_[current_entry_].usri3_flags & UF_PASSWORD_EXPIRED;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isDontExpirePassword() const
{
    return user_info_[current_entry_].usri3_flags & UF_DONT_EXPIRE_PASSWD;
}

//--------------------------------------------------------------------------------------------------
bool UserEnumerator::isLockout() const
{
    return user_info_[current_entry_].usri3_flags & UF_LOCKOUT;
}

//--------------------------------------------------------------------------------------------------
quint32 UserEnumerator::numberLogons() const
{
    return user_info_[current_entry_].usri3_num_logons;
}

//--------------------------------------------------------------------------------------------------
quint32 UserEnumerator::badPasswordCount() const
{
    return user_info_[current_entry_].usri3_bad_pw_count;
}

//--------------------------------------------------------------------------------------------------
quint64 UserEnumerator::lastLogonTime() const
{
    return user_info_[current_entry_].usri3_last_logon;
}

} // base
