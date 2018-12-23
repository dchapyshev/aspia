//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "network/srp_user.h"

namespace aspia {

UserTreeItem::UserTreeItem(size_t index, const SrpUser& user)
    : index_(index)
{
    if (user.flags & SrpUser::ENABLED)
        setIcon(0, QIcon(QStringLiteral(":/icon/user.png")));
    else
        setIcon(0, QIcon(QStringLiteral(":/icon/user-disabled.png")));

    setText(0, user.name);
}

} // namespace aspia
