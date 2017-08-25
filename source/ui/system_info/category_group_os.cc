//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_os.h"
#include "ui/system_info/category_group_users.h"
#include "ui/system_info/category_group_event_log.h"

namespace aspia {

//
// Registration Information
//

class CategoryRegistrationInformation : public Category
{
public:
    CategoryRegistrationInformation();
    ~CategoryRegistrationInformation() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRegistrationInformation);
};

CategoryRegistrationInformation::CategoryRegistrationInformation()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_REGISTRATION_INFO, IDI_COMPUTER)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Task Scheduler
//

class CategoryTaskScheduler : public Category
{
public:
    CategoryTaskScheduler();
    ~CategoryTaskScheduler() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryTaskScheduler);
};

CategoryTaskScheduler::CategoryTaskScheduler()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_TASK_SCHEDULER, IDI_CLOCK)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Environment Variables
//

class CategoryEnvironmentVariables : public Category
{
public:
    CategoryEnvironmentVariables();
    ~CategoryEnvironmentVariables() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEnvironmentVariables);
};

CategoryEnvironmentVariables::CategoryEnvironmentVariables()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_ENVIRONMENT_VARIABLES, IDI_APPLICATIONS)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Operating System Group
//

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
