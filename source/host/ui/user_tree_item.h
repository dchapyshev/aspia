//
// PROJECT:         Aspia
// FILE:            host/ui/user_tree_item.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__USER_TREE_ITEM_H
#define _ASPIA_HOST__UI__USER_TREE_ITEM_H

#include <QTreeWidget>

#include "base/macros.h"
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

    DISALLOW_COPY_AND_ASSIGN(UserTreeItem);
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__USER_TREE_ITEM_H
