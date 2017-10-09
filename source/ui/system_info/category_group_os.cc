//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_os.h"
#include "ui/system_info/category_group_users.h"
#include "ui/system_info/category_group_event_log.h"
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
