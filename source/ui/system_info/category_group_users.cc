//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_users.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_users.h"
#include "ui/system_info/category_info.h"

namespace aspia {

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

CategoryGroupUsers::CategoryGroupUsers()
    : CategoryGroup(IDS_SI_CATEGORY_USERS_AND_GROUPS, IDI_USERS)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryUsers>());
    child_list->emplace_back(std::make_unique<CategoryUserGroups>());
    child_list->emplace_back(std::make_unique<CategoryActiveSessions>());
}

} // namespace aspia
