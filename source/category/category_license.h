//
// PROJECT:         Aspia
// FILE:            category/category_license.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CATEGORY__CATEGORY_LICENSE_H
#define _ASPIA_CATEGORY__CATEGORY_LICENSE_H

#include "category/category.h"

namespace aspia {

class CategoryLicense : public CategoryInfo
{
public:
    CategoryLicense();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryLicense);
};

} // namespace aspia

#endif // _ASPIA_CATEGORY__CATEGORY_LICENSE_H
