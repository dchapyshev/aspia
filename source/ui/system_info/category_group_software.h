//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_software.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_SOFTWARE_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_SOFTWARE_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupSoftware : public CategoryGroup
{
public:
    CategoryGroupSoftware();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupSoftware);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_SOFTWARE_H
