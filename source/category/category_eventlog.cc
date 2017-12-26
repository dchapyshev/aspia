//
// PROJECT:         Aspia
// FILE:            category/category_eventlog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_eventlog.h"
#include "category/category_eventlog.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEventLog::CategoryEventLog()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryEventLog::Name() const
{
    return "Event Logs";
}

Category::IconId CategoryEventLog::Icon() const
{
    return IDI_BOOKS_STACK;
}

const char* CategoryEventLog::Guid() const
{
    return "{0DD03A20-D1AF-4D1F-938F-956EE9003EE9}";
}

void CategoryEventLog::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLog::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
