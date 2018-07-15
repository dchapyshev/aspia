//
// PROJECT:         Aspia
// FILE:            client/ui/category_group_tree_item.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__CATEGORY_GROUP_TREE_ITEM_H
#define _ASPIA_CLIENT__UI__CATEGORY_GROUP_TREE_ITEM_H

#include <QTreeWidget>

#include "system_info/category.h"

namespace aspia {

class CategoryGroupTreeItem : public QTreeWidgetItem
{
public:
    CategoryGroupTreeItem(const CategoryGroup& category_group);

    const CategoryGroup& categoryGroup() const { return category_group_; }

private:
    CategoryGroup category_group_;

    Q_DISABLE_COPY(CategoryGroupTreeItem)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__CATEGORY_GROUP_TREE_ITEM_H
