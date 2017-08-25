//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_dmi.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DMI_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DMI_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupDMI : public CategoryGroup
{
public:
    CategoryGroupDMI();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDMI);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DMI_H
