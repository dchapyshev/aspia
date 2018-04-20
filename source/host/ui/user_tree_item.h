//
// PROJECT:         Aspia
// FILE:            host/ui/user_tree_item.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__USER_TREE_ITEM_H
#define _ASPIA_HOST__UI__USER_TREE_ITEM_H

#include <QTreeWidget>

#include "host/user.h"

namespace aspia {

class UserTreeItem : public QTreeWidgetItem
{
public:
    UserTreeItem(int index, User* user);
    ~UserTreeItem() = default;

    User* user() const { return user_; }
    int userIndex() const { return index_; }

private:
    User* user_;
    int index_;

    Q_DISABLE_COPY(UserTreeItem)
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__USER_TREE_ITEM_H
