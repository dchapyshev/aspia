//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_users.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_users.h"

namespace aspia {

//
// Users
//

class CategoryUsers : public Category
{
public:
    CategoryUsers();
    ~CategoryUsers() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUsers);
};

CategoryUsers::CategoryUsers()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_USERS, IDI_USER)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// User Groups
//

class CategoryUserGroups : public Category
{
public:
    CategoryUserGroups();
    ~CategoryUserGroups() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUserGroups);
};

CategoryUserGroups::CategoryUserGroups()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_USER_GROUPS, IDI_USERS)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Active Sessions
//

class CategoryActiveSessions : public Category
{
public:
    CategoryActiveSessions();
    ~CategoryActiveSessions() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryActiveSessions);
};

CategoryActiveSessions::CategoryActiveSessions()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_ACTIVE_SESSIONS, IDI_USERS)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Users and user groups
//

CategoryGroupUsers::CategoryGroupUsers()
    : CategoryGroup(IDS_SI_CATEGORY_USERS_AND_GROUPS, IDI_USERS)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryUsers>());
    child_list->emplace_back(std::make_unique<CategoryUserGroups>());
    child_list->emplace_back(std::make_unique<CategoryActiveSessions>());
}

} // namespace aspia
