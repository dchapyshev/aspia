//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_os.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryRegistrationInformation : public CategoryInfo
{
public:
    CategoryRegistrationInformation()
        : CategoryInfo(system_info::operating_system::kRegistrationInformation,
                       IDS_SI_CATEGORY_REGISTRATION_INFO,
                       IDI_COMPUTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryRegistrationInformation() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRegistrationInformation);
};

class CategoryTaskScheduler : public CategoryInfo
{
public:
    CategoryTaskScheduler()
        : CategoryInfo(system_info::operating_system::kTaskScheduler,
                       IDS_SI_CATEGORY_TASK_SCHEDULER,
                       IDI_CLOCK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryTaskScheduler() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryTaskScheduler);
};

class CategoryEnvironmentVariables : public CategoryInfo
{
public:
    CategoryEnvironmentVariables()
        : CategoryInfo(system_info::operating_system::kEnvironmentVariables,
                       IDS_SI_CATEGORY_ENVIRONMENT_VARIABLES,
                       IDI_APPLICATIONS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryEnvironmentVariables() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEnvironmentVariables);
};

class CategoryApplications : public CategoryInfo
{
public:
    CategoryApplications()
        : CategoryInfo(system_info::operating_system::event_logs::kApplications,
                       IDS_SI_CATEGORY_EVENT_LOG_APPLICATIONS,
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryApplications() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryApplications);
};

class CategorySecurity : public CategoryInfo
{
public:
    CategorySecurity()
        : CategoryInfo(system_info::operating_system::event_logs::kSecurity,
                       IDS_SI_CATEGORY_EVENT_LOG_SECURITY,
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategorySecurity() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySecurity);
};

class CategorySystem : public CategoryInfo
{
public:
    CategorySystem()
        : CategoryInfo(system_info::operating_system::event_logs::kSystem,
                       IDS_SI_CATEGORY_EVENT_LOG_SYSTEM,
                       IDI_ERROR_LOG)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategorySystem() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySystem);
};

class CategoryGroupEventLog : public CategoryGroup
{
public:
    CategoryGroupEventLog()
        : CategoryGroup(IDS_SI_CATEGORY_EVENT_LOGS, IDI_BOOKS_STACK)
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
                       IDS_SI_CATEGORY_USERS,
                       IDI_USER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryUsers() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUsers);
};

class CategoryUserGroups : public CategoryInfo
{
public:
    CategoryUserGroups()
        : CategoryInfo(system_info::operating_system::users_and_groups::kUserGroups,
                       IDS_SI_CATEGORY_USER_GROUPS,
                       IDI_USERS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryUserGroups() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUserGroups);
};

class CategoryActiveSessions : public CategoryInfo
{
public:
    CategoryActiveSessions()
        : CategoryInfo(system_info::operating_system::users_and_groups::kActiveSessions,
                       IDS_SI_CATEGORY_ACTIVE_SESSIONS,
                       IDI_USERS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryActiveSessions() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryActiveSessions);
};

class CategoryGroupUsers : public CategoryGroup
{
public:
    CategoryGroupUsers()
        : CategoryGroup(IDS_SI_CATEGORY_USERS_AND_GROUPS, IDI_USERS)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryUsers>());
        child_list->emplace_back(std::make_unique<CategoryUserGroups>());
        child_list->emplace_back(std::make_unique<CategoryActiveSessions>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupUsers);
};

CategoryGroupOS::CategoryGroupOS()
    : CategoryGroup(IDS_SI_CATEGORY_OS, IDI_OS)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryRegistrationInformation>());
    child_list->emplace_back(std::make_unique<CategoryTaskScheduler>());
    child_list->emplace_back(std::make_unique<CategoryGroupUsers>());
    child_list->emplace_back(std::make_unique<CategoryEnvironmentVariables>());
    child_list->emplace_back(std::make_unique<CategoryGroupEventLog>());
}

} // namespace aspia
