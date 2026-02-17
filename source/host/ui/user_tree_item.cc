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

#include "host/ui/user_tree_item.h"

namespace host {

//--------------------------------------------------------------------------------------------------
UserTreeItem::UserTreeItem(const base::User& user)
    : user_(user)
{
    updateData();
}

//--------------------------------------------------------------------------------------------------
void UserTreeItem::setUser(const base::User& user)
{
    user_ = user;
    updateData();
}

//--------------------------------------------------------------------------------------------------
void UserTreeItem::updateData()
{
    if (user_.flags & base::User::ENABLED)
        setIcon(0, QIcon(":/img/user.svg"));
    else
        setIcon(0, QIcon(":/img/locked-user.svg"));

    setText(0, user_.name);
}

} // namespace host
