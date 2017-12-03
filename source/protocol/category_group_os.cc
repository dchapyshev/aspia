//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/user_enumerator.h"
#include "base/user_group_enumerator.h"
#include "base/session_enumerator.h"
#include "base/datetime.h"
#include "protocol/category_group_os.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryRegistrationInformation
//

const char* CategoryRegistrationInformation::Name() const
{
    return "Registration Information";
}

Category::IconId CategoryRegistrationInformation::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategoryRegistrationInformation::Guid() const
{
    return "2DDA7127-6ECF-49E1-9C6A-549AEF4B9E87";
}

void CategoryRegistrationInformation::Parse(Output* /* output */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryRegistrationInformation::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryTaskScheduler
//

const char* CategoryTaskScheduler::Name() const
{
    return "Task Scheduler";
}

Category::IconId CategoryTaskScheduler::Icon() const
{
    return IDI_CLOCK;
}

const char* CategoryTaskScheduler::Guid() const
{
    return "1B27C27F-847E-47CC-92DF-6B8F5CB4827A";
}

void CategoryTaskScheduler::Parse(Output* /* output */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryTaskScheduler::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryEnvironmentVariables
//

const char* CategoryEnvironmentVariables::Name() const
{
    return "Environment Variables";
}

Category::IconId CategoryEnvironmentVariables::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryEnvironmentVariables::Guid() const
{
    return "AAB8670A-3C90-4F75-A907-512ACBAD1BE6";
}

void CategoryEnvironmentVariables::Parse(Output* /* output */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEnvironmentVariables::Serialize()
{
    // TODO
    return std::string();
}

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
    return "0DD03A20-D1AF-4D1F-938F-956EE9003EE9";
}

void CategoryEventLogsApplications::Parse(Output* /* output */, const std::string& /* data */)
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
    return "7E0220A8-AC51-4C9E-8834-F0F805D40977";
}

void CategoryEventLogsSecurity::Parse(Output* /* output */, const std::string& /* data */)
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
    return "8421A38A-4757-4298-A5CB-9493C7726515";
}

void CategoryEventLogsSystem::Parse(Output* /* output */, const std::string& /* data */)
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
// CategoryUsers
//

const char* CategoryUsers::Name() const
{
    return "Users";
}

Category::IconId CategoryUsers::Icon() const
{
    return IDI_USER;
}

const char* CategoryUsers::Guid() const
{
    return "838AD22A-82BB-47F2-9001-1CD9714ED298";
}

void CategoryUsers::Parse(Output* output, const std::string& data)
{
    proto::Users message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Users::Item& item = message.item(index);

        Output::Group group(output, item.name(), Icon());

        if (!item.full_name().empty())
        {
            output->AddParam(IDI_USER, "Full Name", item.full_name());
        }

        if (!item.comment().empty())
        {
            output->AddParam(IDI_DOCUMENT_TEXT, "Description", item.comment());
        }

        output->AddParam(item.is_disabled() ? IDI_CHECKED : IDI_UNCHECKED,
                         "Disabled",
                         item.is_disabled() ? "Yes" : "No");

        output->AddParam(item.is_password_cant_change() ? IDI_CHECKED : IDI_UNCHECKED,
                         "Password Can't Change",
                         item.is_password_cant_change() ? "Yes" : "No");

        output->AddParam(item.is_password_expired() ? IDI_CHECKED : IDI_UNCHECKED,
                         "Password Expired",
                         item.is_password_expired() ? "Yes" : "No");

        output->AddParam(item.is_dont_expire_password() ? IDI_CHECKED : IDI_UNCHECKED,
                         "Don't Expire Password",
                         item.is_dont_expire_password() ? "Yes" : "No");

        output->AddParam(item.is_lockout() ? IDI_CHECKED : IDI_UNCHECKED,
                         "Lockout",
                         item.is_lockout() ? "Yes" : "No");

        output->AddParam(IDI_CLOCK, "Last Logon", TimeToString(item.last_logon_time()));
        output->AddParam(IDI_COUNTER, "Number Logons", std::to_string(item.number_logons()));

        output->AddParam(IDI_COUNTER,
                         "Bad Password Count",
                         std::to_string(item.bad_password_count()));
    }
}

std::string CategoryUsers::Serialize()
{
    proto::Users message;

    for (UserEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Users::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_full_name(enumerator.GetFullName());
        item->set_comment(enumerator.GetComment());
        item->set_is_disabled(enumerator.IsDisabled());
        item->set_is_password_cant_change(enumerator.IsPasswordCantChange());
        item->set_is_password_expired(enumerator.IsPasswordExpired());
        item->set_is_dont_expire_password(enumerator.IsDontExpirePassword());
        item->set_is_lockout(enumerator.IsLockout());
        item->set_number_logons(enumerator.GetNumberLogons());
        item->set_bad_password_count(enumerator.GetBadPasswordCount());
        item->set_last_logon_time(enumerator.GetLastLogonTime());
    }

    return message.SerializeAsString();
}

//
// CategoryUserGroups
//

const char* CategoryUserGroups::Name() const
{
    return "User Groups";
}

Category::IconId CategoryUserGroups::Icon() const
{
    return IDI_USERS;
}

const char* CategoryUserGroups::Guid() const
{
    return "B560FDED-5E88-4116-98A5-12462C07AC90";
}

void CategoryUserGroups::Parse(Output* output, const std::string& data)
{
    proto::UserGroups message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Group Name", 250);
        output->AddHeaderItem("Description", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::UserGroups::Item& item = message.item(index);

        Output::Row row(output, Icon());

        output->AddValue(item.name());
        output->AddValue(item.comment());
    }
}

std::string CategoryUserGroups::Serialize()
{
    proto::UserGroups message;

    for (UserGroupEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::UserGroups::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_comment(enumerator.GetComment());
    }

    return message.SerializeAsString();
}

//
// CategoryActiveSessions
//

const char* CategoryActiveSessions::Name() const
{
    return "Active Sessions";
}

Category::IconId CategoryActiveSessions::Icon() const
{
    return IDI_USERS;
}

const char* CategoryActiveSessions::Guid() const
{
    return "8702E4A1-C9A2-4BA3-BBDE-CFCB6937D2C8";
}

void CategoryActiveSessions::Parse(Output* output, const std::string& data)
{
    proto::Sessions message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("User Name", 150);
        output->AddHeaderItem("Domain", 100);
        output->AddHeaderItem("ID", 50);
        output->AddHeaderItem("State", 80);
        output->AddHeaderItem("Client Name", 100);
        output->AddHeaderItem("Logon Type", 100);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Sessions::Item& item = message.item(index);

        Output::Row row(output, Icon());

        output->AddValue(item.user_name());
        output->AddValue(item.domain_name());
        output->AddValue(std::to_string(item.session_id()));
        output->AddValue(item.connect_state());
        output->AddValue(item.client_name());
        output->AddValue(item.winstation_name());
    }
}

std::string CategoryActiveSessions::Serialize()
{
    proto::Sessions message;

    for (SessionEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Sessions::Item* item = message.add_item();

        item->set_user_name(enumerator.GetUserName());
        item->set_domain_name(enumerator.GetDomainName());
        item->set_session_id(enumerator.GetSessionId());
        item->set_connect_state(enumerator.GetConnectState());
        item->set_client_name(enumerator.GetClientName());
        item->set_winstation_name(enumerator.GetWinStationName());
    }

    return message.SerializeAsString();
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
