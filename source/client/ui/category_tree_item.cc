//
// PROJECT:         Aspia
// FILE:            client/ui/category_tree_item.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/category_tree_item.h"

namespace aspia {

CategoryTreeItem::CategoryTreeItem(const Category& category)
    : category_(category)
{
    setIcon(0, category_.icon());
    setText(0, category_.name());
}

} // namespace aspia
