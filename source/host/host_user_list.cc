//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_user_list.h"

#include "host/database.h"

//--------------------------------------------------------------------------------------------------
User HostUserList::find(const QString& username) const
{
    User user(Database::instance().findUser(username));
    if (user.isValid())
        return user;

    if (one_time_user_.isValid() && one_time_user_.name.compare(username, Qt::CaseInsensitive) == 0)
        return one_time_user_;

    return User();
}

//--------------------------------------------------------------------------------------------------
QByteArray HostUserList::seedKey() const
{
    return Database::instance().seedKey();
}

//--------------------------------------------------------------------------------------------------
void HostUserList::setSeedKey(const QByteArray& seed_key)
{
    Database::instance().setSeedKey(seed_key);
}

//--------------------------------------------------------------------------------------------------
void HostUserList::setOneTimeUser(const User& user)
{
    one_time_user_ = user;
}
