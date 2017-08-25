//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_os.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_OS_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_OS_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupOS : public CategoryGroup
{
public:
    CategoryGroupOS();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupOS);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_OS_H
