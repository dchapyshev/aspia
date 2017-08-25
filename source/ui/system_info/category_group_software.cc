//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_software.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_software.h"

namespace aspia {

//
// Programs
//

class CategoryPrograms : public Category
{
public:
    CategoryPrograms();
    ~CategoryPrograms() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrograms);
};

CategoryPrograms::CategoryPrograms()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_SOFTWARE_PROGRAMS, IDI_APPLICATIONS)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Updates
//

class CategoryUpdates : public Category
{
public:
    CategoryUpdates();
    ~CategoryUpdates() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUpdates);
};

CategoryUpdates::CategoryUpdates()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_SOFTWARE_UPDATES, IDI_APPLICATIONS)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Services
//

class CategoryServices : public Category
{
public:
    CategoryServices();
    ~CategoryServices() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryServices);
};

CategoryServices::CategoryServices()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_SERVICES, IDI_GEAR)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Drivers
//

class CategoryDrivers : public Category
{
public:
    CategoryDrivers();
    ~CategoryDrivers() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDrivers);
};

CategoryDrivers::CategoryDrivers()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_DRIVERS, IDI_PCI)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Processes
//

class CategoryProcesses : public Category
{
public:
    CategoryProcesses();
    ~CategoryProcesses() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryProcesses);
};

CategoryProcesses::CategoryProcesses()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_PROCESSES, IDI_SYSTEM_MONITOR)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Licenses
//

class CategoryLicenses : public Category
{
public:
    CategoryLicenses();
    ~CategoryLicenses() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryLicenses);
};

CategoryLicenses::CategoryLicenses()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_LICENSES, IDI_LICENSE_KEY)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Software Group
//

CategoryGroupSoftware::CategoryGroupSoftware()
    : CategoryGroup(IDS_SI_CATEGORY_SOFTWARE, IDI_SOFTWARE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryPrograms>());
    child_list->emplace_back(std::make_unique<CategoryUpdates>());
    child_list->emplace_back(std::make_unique<CategoryServices>());
    child_list->emplace_back(std::make_unique<CategoryDrivers>());
    child_list->emplace_back(std::make_unique<CategoryProcesses>());
    child_list->emplace_back(std::make_unique<CategoryLicenses>());
}

} // namespace aspia
