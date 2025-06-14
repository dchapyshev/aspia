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

#include "base/win/user_group_enumerator.h"

#include "base/logging.h"
#include "base/system_error.h"

#include <qt_windows.h>
#include <lm.h>

namespace base {

//--------------------------------------------------------------------------------------------------
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
        LOG(ERROR) << "NetLocalGroupEnum failed:" << SystemError(error_code).toString();
        return;
    }
}

//--------------------------------------------------------------------------------------------------
UserGroupEnumerator::~UserGroupEnumerator()
{
    if (group_info_)
        NetApiBufferFree(group_info_);
}

//--------------------------------------------------------------------------------------------------
void UserGroupEnumerator::advance()
{
    ++current_entry_;
}

//--------------------------------------------------------------------------------------------------
bool UserGroupEnumerator::isAtEnd() const
{
    return current_entry_ >= total_entries_;
}

//--------------------------------------------------------------------------------------------------
QString UserGroupEnumerator::name() const
{
    if (!group_info_[current_entry_].lgrpi1_name)
        return QString();
    return QString::fromWCharArray(group_info_[current_entry_].lgrpi1_name);
}

//--------------------------------------------------------------------------------------------------
QString UserGroupEnumerator::comment() const
{
    if (!group_info_[current_entry_].lgrpi1_comment)
        return QString();
    return QString::fromWCharArray(group_info_[current_entry_].lgrpi1_comment);
}

} // base
