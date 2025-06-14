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

#ifndef BASE_WIN_USER_GROUP_ENUMERATOR_H
#define BASE_WIN_USER_GROUP_ENUMERATOR_H

#include <QString>

struct _LOCALGROUP_INFO_1;

namespace base {

class UserGroupEnumerator
{
public:
    UserGroupEnumerator();
    ~UserGroupEnumerator();

    void advance();
    bool isAtEnd() const;

    QString name() const;
    QString comment() const;

private:
    _LOCALGROUP_INFO_1* group_info_ = nullptr;
    unsigned long total_entries_ = 0;
    unsigned long current_entry_ = 0;

    Q_DISABLE_COPY(UserGroupEnumerator)
};

} // base

#endif // BASE_WIN_USER_GROUP_ENUMERATOR_H
