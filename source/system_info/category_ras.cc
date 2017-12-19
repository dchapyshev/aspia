//
// PROJECT:         Aspia
// FILE:            system_info/category_ras.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_ras.h"
#include "system_info/category_ras.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryRAS::CategoryRAS()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryRAS::Name() const
{
    return "RAS Connections";
}

Category::IconId CategoryRAS::Icon() const
{
    return IDI_TELEPHONE_FAX;
}

const char* CategoryRAS::Guid() const
{
    return "{E0A43CFD-3A97-4577-B3FB-3B542C0729F7}";
}

void CategoryRAS::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryRAS::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
