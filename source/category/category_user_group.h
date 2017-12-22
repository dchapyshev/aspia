//
// PROJECT:         Aspia
// FILE:            category/category_user_group.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CATEGORY__CATEGORY_USER_GROUP_H
#define _ASPIA_CATEGORY__CATEGORY_USER_GROUP_H

#include "category/category.h"

namespace aspia {

class CategoryUserGroup : public CategoryInfo
{
public:
    CategoryUserGroup();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUserGroup);
};

} // namespace aspia

#endif // _ASPIA_CATEGORY__CATEGORY_USER_GROUP_H
