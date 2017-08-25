//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_storage.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_STORAGE_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_STORAGE_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupStorage : public CategoryGroup
{
public:
    CategoryGroupStorage();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupStorage);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_STORAGE_H
