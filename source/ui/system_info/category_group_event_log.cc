//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_event_log.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_event_log.h"

namespace aspia {

//
// Applications
//

class CategoryApplications : public Category
{
public:
    CategoryApplications();
    ~CategoryApplications() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryApplications);
};

CategoryApplications::CategoryApplications()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_EVENT_LOG_APPLICATIONS, IDI_ERROR_LOG)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Security
//

class CategorySecurity : public Category
{
public:
    CategorySecurity();
    ~CategorySecurity() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySecurity);
};

CategorySecurity::CategorySecurity()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_EVENT_LOG_SECURITY, IDI_ERROR_LOG)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// System
//

class CategorySystem : public Category
{
public:
    CategorySystem();
    ~CategorySystem() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySystem);
};

CategorySystem::CategorySystem()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_EVENT_LOG_SYSTEM, IDI_ERROR_LOG)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Event Log Group
//

CategoryGroupEventLog::CategoryGroupEventLog()
    : CategoryGroup(IDS_SI_CATEGORY_EVENT_LOGS, IDI_BOOKS_STACK)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryApplications>());
    child_list->emplace_back(std::make_unique<CategorySecurity>());
    child_list->emplace_back(std::make_unique<CategorySystem>());
}

} // namespace aspia
