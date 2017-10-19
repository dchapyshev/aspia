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
#include "protocol/system_info_constants.h"
#include "protocol/category_group_os.h"
#include "protocol/category_info.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

class CategoryRegistrationInformation : public CategoryInfo
{
public:
    CategoryRegistrationInformation()
        : CategoryInfo(system_info::operating_system::kRegistrationInformation,
                       "Registration Information",
                       IDI_COMPUTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRegistrationInformation);
};

class CategoryTaskScheduler : public CategoryInfo
{
public:
    CategoryTaskScheduler()
        : CategoryInfo(system_info::operating_system::kTaskScheduler,
                       "Task Scheduler",
                       IDI_CLOCK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryTaskScheduler);
};

class CategoryEnvironmentVariables : public CategoryInfo
{
public:
    CategoryEnvironmentVariables()
        : CategoryInfo(system_info::operating_system::kEnvironmentVariables,
                       "Environment Variables",
                       IDI_APPLICATIONS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEnvironmentVariables);
};

class CategoryApplications : public CategoryInfo
{
public:
    CategoryApplications()
        : CategoryInfo(system_info::operating_system::event_logs::kApplications,
                       "Applications",
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryApplications);
};

class CategorySecurity : public CategoryInfo
{
public:
    CategorySecurity()
        : CategoryInfo(system_info::operating_system::event_logs::kSecurity,
                       "Security",
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySecurity);
};

class CategorySystem : public CategoryInfo
{
public:
    CategorySystem()
        : CategoryInfo(system_info::operating_system::event_logs::kSystem,
                       "System",
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySystem);
};

class CategoryGroupEventLog : public CategoryGroup
{
public:
    CategoryGroupEventLog() : CategoryGroup("Event Logs", IDI_BOOKS_STACK)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryApplications>());
        child_list->emplace_back(std::make_unique<CategorySecurity>());
        child_list->emplace_back(std::make_unique<CategorySystem>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupEventLog);
};

class CategoryUsers : public CategoryInfo
{
public:
    CategoryUsers()
        : CategoryInfo(system_info::operating_system::users_and_groups::kUsers,
                      "Users",
                       IDI_USER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Users message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::Users::Item& item = message.item(index);

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

    std::string Serialize() final
    {
        system_info::Users message;

        for (UserEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::Users::Item* item = message.add_item();

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

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUsers);
};

class CategoryUserGroups : public CategoryInfo
{
public:
    CategoryUserGroups()
        : CategoryInfo(system_info::operating_system::users_and_groups::kUserGroups,
                       "User Groups",
                       IDI_USERS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Group Name", 250);
        column_list->emplace_back("Description", 250);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::UserGroups message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::UserGroups::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.name());
            output->AddValue(item.comment());
        }
    }

    std::string Serialize() final
    {
        system_info::UserGroups message;

        for (UserGroupEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::UserGroups::Item* item = message.add_item();

            item->set_name(enumerator.GetName());
            item->set_comment(enumerator.GetComment());
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUserGroups);
};

class CategoryActiveSessions : public CategoryInfo
{
public:
    CategoryActiveSessions()
        : CategoryInfo(system_info::operating_system::users_and_groups::kActiveSessions,
                       "Active Sessions",
                       IDI_USERS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("User Name", 150);
        column_list->emplace_back("Domain", 100);
        column_list->emplace_back("ID", 50);
        column_list->emplace_back("State", 80);
        column_list->emplace_back("Client Name", 100);
        column_list->emplace_back("Logon Type", 100);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Sessions message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::Sessions::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.user_name());
            output->AddValue(item.domain_name());
            output->AddValue(std::to_string(item.session_id()));
            output->AddValue(item.connect_state());
            output->AddValue(item.client_name());
            output->AddValue(item.winstation_name());
        }
    }

    std::string Serialize() final
    {
        system_info::Sessions message;

        for (SessionEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::Sessions::Item* item = message.add_item();

            item->set_user_name(enumerator.GetUserName());
            item->set_domain_name(enumerator.GetDomainName());
            item->set_session_id(enumerator.GetSessionId());
            item->set_connect_state(enumerator.GetConnectState());
            item->set_client_name(enumerator.GetClientName());
            item->set_winstation_name(enumerator.GetWinStationName());
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryActiveSessions);
};

class CategoryGroupUsers : public CategoryGroup
{
public:
    CategoryGroupUsers() : CategoryGroup("Users and groups", IDI_USERS)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryUsers>());
        child_list->emplace_back(std::make_unique<CategoryUserGroups>());
        child_list->emplace_back(std::make_unique<CategoryActiveSessions>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupUsers);
};

CategoryGroupOS::CategoryGroupOS() : CategoryGroup("Operating System", IDI_OS)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryRegistrationInformation>());
    child_list->emplace_back(std::make_unique<CategoryTaskScheduler>());
    child_list->emplace_back(std::make_unique<CategoryGroupUsers>());
    child_list->emplace_back(std::make_unique<CategoryEnvironmentVariables>());
    child_list->emplace_back(std::make_unique<CategoryGroupEventLog>());
}

} // namespace aspia
