//
// PROJECT:         Aspia
// FILE:            client/ui/category_tree_item.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H
#define _ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H

#include <QTreeWidget>

#include "system_info/category.h"

namespace aspia {

class CategoryTreeItem : public QTreeWidgetItem
{
public:
    CategoryTreeItem(const Category& category);

    const Category& category() const { return category_; }

private:
    Category category_;

    Q_DISABLE_COPY(CategoryTreeItem)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H
