//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_windows_devices.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_WINDOWS_DEVICES_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_WINDOWS_DEVICES_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupWindowDevices : public CategoryGroup
{
public:
    CategoryGroupWindowDevices();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupWindowDevices);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_WINDOWS_DEVICES_H
