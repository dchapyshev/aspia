//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_display.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DISPLAY_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DISPLAY_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupDisplay : public CategoryGroup
{
public:
    CategoryGroupDisplay();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDisplay);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_DISPLAY_H
