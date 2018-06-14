//
// PROJECT:         Aspia
// FILE:            host/ui/user_tree_item.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ui/user_tree_item.h"

namespace aspia {

UserTreeItem::UserTreeItem(int index, User* user)
    : index_(index),
      user_(user)
{
    if (user_->flags() & User::FLAG_ENABLED)
        setIcon(0, QIcon(QStringLiteral(":/icon/user.png")));
    else
        setIcon(0, QIcon(QStringLiteral(":/icon/user-disabled.png")));

    setText(0, user_->name());
}

} // namespace aspia
