//
// PROJECT:         Aspia
// FILE:            category/category_eventlog_security.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_eventlog_security.h"
#include "category/category_eventlog_security.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEventLogSecurity::CategoryEventLogSecurity()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryEventLogSecurity::Name() const
{
    return "Security";
}

Category::IconId CategoryEventLogSecurity::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogSecurity::Guid() const
{
    return "{7E0220A8-AC51-4C9E-8834-F0F805D40977}";
}

void CategoryEventLogSecurity::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogSecurity::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
