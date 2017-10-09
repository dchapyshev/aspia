//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_event_log.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_event_log.h"
#include "ui/system_info/category_info.h"

namespace aspia {

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
