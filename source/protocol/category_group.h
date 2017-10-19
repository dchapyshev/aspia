//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_H

#include "base/macros.h"
#include "protocol/category.h"

namespace aspia {

class CategoryGroup : public Category
{
public:
    CategoryGroup(const std::string& name, IconId icon_id)
        : Category(Type::GROUP, name, icon_id)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Category Name", 250);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroup);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_H
