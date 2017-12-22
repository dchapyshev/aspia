//
// PROJECT:         Aspia
// FILE:            category/category_license.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_license.h"
#include "category/category_license.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryLicense::CategoryLicense()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryLicense::Name() const
{
    return "Licenses";
}

Category::IconId CategoryLicense::Icon() const
{
    return IDI_LICENSE_KEY;
}

const char* CategoryLicense::Guid() const
{
    return "{6BD88575-9D23-44BC-8A49-64D94CC3EE48}";
}

void CategoryLicense::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryLicense::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
