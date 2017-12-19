//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_baseboard.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__CATEGORY_DMI_BASEBOARD_H
#define _ASPIA_SYSTEM_INFO__CATEGORY_DMI_BASEBOARD_H

#include "system_info/category.h"

namespace aspia {

class CategoryDmiBaseboard : public CategoryInfo
{
public:
    CategoryDmiBaseboard();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBaseboard);
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__CATEGORY_DMI_BASEBOARD_H
