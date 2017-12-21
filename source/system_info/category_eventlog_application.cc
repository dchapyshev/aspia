//
// PROJECT:         Aspia
// FILE:            system_info/category_eventlog_application.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_eventlog_application.h"
#include "system_info/category_eventlog_application.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEventLogApplication::CategoryEventLogApplication()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryEventLogApplication::Name() const
{
    return "Applications";
}

Category::IconId CategoryEventLogApplication::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogApplication::Guid() const
{
    return "{0DD03A20-D1AF-4D1F-938F-956EE9003EE9}";
}

void CategoryEventLogApplication::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogApplication::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
