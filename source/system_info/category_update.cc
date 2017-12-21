//
// PROJECT:         Aspia
// FILE:            system_info/category_update.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_update.h"
#include "system_info/category_update.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryUpdate::CategoryUpdate()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryUpdate::Name() const
{
    return "Updates";
}

Category::IconId CategoryUpdate::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryUpdate::Guid() const
{
    return "{3E160E27-BE2E-45DB-8292-C3786C9533AB}";
}

void CategoryUpdate::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryUpdate::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
