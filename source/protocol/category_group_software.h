//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_software.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H

#include "protocol/category_group.h"

namespace aspia {

class CategoryGroupSoftware : public CategoryGroup
{
public:
    CategoryGroupSoftware();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupSoftware);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H
