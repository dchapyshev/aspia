//
// PROJECT:         Aspia
// FILE:            client/ui/category_group_tree_item.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/category_group_tree_item.h"

namespace aspia {

CategoryGroupTreeItem::CategoryGroupTreeItem(const CategoryGroup& category_group)
    : category_group_(category_group)
{
    setIcon(0, category_group_.icon());
    setText(0, category_group_.name());
}

} // namespace aspia
