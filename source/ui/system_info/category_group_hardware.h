//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_hardware.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_HARDWARE_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_HARDWARE_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupHardware : public CategoryGroup
{
public:
    CategoryGroupHardware();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupHardware);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_HARDWARE_H
