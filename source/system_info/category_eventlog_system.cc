//
// PROJECT:         Aspia
// FILE:            system_info/category_eventlog_system.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_eventlog_system.h"
#include "system_info/category_eventlog_system.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEventLogSystem::CategoryEventLogSystem()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryEventLogSystem::Name() const
{
    return "System";
}

Category::IconId CategoryEventLogSystem::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogSystem::Guid() const
{
    return "{8421A38A-4757-4298-A5CB-9493C7726515}";
}

void CategoryEventLogSystem::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogSystem::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
