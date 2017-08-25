//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_H

#include "base/macros.h"
#include "ui/system_info/category.h"
#include "ui/resource.h"

namespace aspia {

class CategoryGroup : public Category
{
public:
    CategoryGroup(UINT name_id, UINT icon_id)
        : Category(Type::GROUP, name_id, icon_id)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_CATEGORY_NAME, 250);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroup);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_H
