//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_INFO_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_INFO_H

#include "ui/system_info/category.h"
#include "ui/system_info/output.h"

namespace aspia {

class CategoryInfo : public Category
{
public:
    CategoryInfo(const char* guid, const std::string& name, IconId icon_id)
        : Category(Type::INFO, name, icon_id),
          guid_(guid)
    {
        DCHECK(guid);
    }

    virtual ~CategoryInfo() = default;
    virtual void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) = 0;
    virtual std::string Serialize() = 0;

    const char* guid() const { return guid_; }

private:
    const char* guid_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_INFO_H
