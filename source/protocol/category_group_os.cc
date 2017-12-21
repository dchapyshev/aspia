//
// PROJECT:         Aspia
// FILE:            protocol/category_group_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/user_enumerator.h"
#include "base/user_group_enumerator.h"
#include "base/datetime.h"
#include "protocol/category_group_os.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryEventLogsApplications
//

const char* CategoryEventLogsApplications::Name() const
{
    return "Applications";
}

Category::IconId CategoryEventLogsApplications::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogsApplications::Guid() const
{
    return "{0DD03A20-D1AF-4D1F-938F-956EE9003EE9}";
}

void CategoryEventLogsApplications::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogsApplications::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryEventLogsSecurity
//

const char* CategoryEventLogsSecurity::Name() const
{
    return "Security";
}

Category::IconId CategoryEventLogsSecurity::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogsSecurity::Guid() const
{
    return "{7E0220A8-AC51-4C9E-8834-F0F805D40977}";
}

void CategoryEventLogsSecurity::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogsSecurity::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryEventLogsSystem
//

const char* CategoryEventLogsSystem::Name() const
{
    return "System";
}

Category::IconId CategoryEventLogsSystem::Icon() const
{
    return IDI_ERROR_LOG;
}

const char* CategoryEventLogsSystem::Guid() const
{
    return "{8421A38A-4757-4298-A5CB-9493C7726515}";
}

void CategoryEventLogsSystem::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEventLogsSystem::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupEventLog
//

const char* CategoryGroupEventLog::Name() const
{
    return "Event Logs";
}

Category::IconId CategoryGroupEventLog::Icon() const
{
    return IDI_BOOKS_STACK;
}

//
// CategoryGroupUsers
//

const char* CategoryGroupUsers::Name() const
{
    return "Users and groups";
}

Category::IconId CategoryGroupUsers::Icon() const
{
    return IDI_USERS;
}

//
// CategoryGroupOS
//

const char* CategoryGroupOS::Name() const
{
    return "Operating System";
}

Category::IconId CategoryGroupOS::Icon() const
{
    return IDI_OS;
}

} // namespace aspia
