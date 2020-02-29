//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/user.h"

namespace host {

UserTreeItem::UserTreeItem(const User& user)
    : user_(user)
{
    updateData();
}

void UserTreeItem::setUser(const User& user)
{
    user_ = user;
    updateData();
}

void UserTreeItem::updateData()
{
    if (user_.flags & User::ENABLED)
        setIcon(0, QIcon(QLatin1String(":/img/user.png")));
    else
        setIcon(0, QIcon(QLatin1String(":/img/user-disabled.png")));

    setText(0, QString::fromStdU16String(user_.name));
}

} // namespace host
