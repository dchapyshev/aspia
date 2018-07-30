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

#ifndef ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H_
#define ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H_

#include <QTreeWidget>

#include "base/macros_magic.h"
#include "system_info/category.h"

namespace aspia {

class CategoryTreeItem : public QTreeWidgetItem
{
public:
    CategoryTreeItem(const Category& category);

    const Category& category() const { return category_; }

private:
    Category category_;

    DISALLOW_COPY_AND_ASSIGN(CategoryTreeItem);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__CATEGORY_TREE_ITEM_H_
