//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_software.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_software.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryPrograms : public CategoryInfo
{
public:
    CategoryPrograms()
        : CategoryInfo(system_info::software::kPrograms,
                       IDS_SI_CATEGORY_SOFTWARE_PROGRAMS,
                       IDI_APPLICATIONS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryPrograms() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrograms);
};

class CategoryUpdates : public CategoryInfo
{
public:
    CategoryUpdates()
        : CategoryInfo(system_info::software::kUpdates,
                       IDS_SI_CATEGORY_SOFTWARE_UPDATES,
                       IDI_APPLICATIONS)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryUpdates() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUpdates);
};

class CategoryServices : public CategoryInfo
{
public:
    CategoryServices()
        : CategoryInfo(system_info::software::kServices,
                       IDS_SI_CATEGORY_SERVICES,
                       IDI_GEAR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryServices() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryServices);
};

class CategoryDrivers : public CategoryInfo
{
public:
    CategoryDrivers()
        : CategoryInfo(system_info::software::kDrivers,
                       IDS_SI_CATEGORY_DRIVERS,
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryDrivers() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDrivers);
};

class CategoryProcesses : public CategoryInfo
{
public:
    CategoryProcesses()
        : CategoryInfo(system_info::software::kProcesses,
                       IDS_SI_CATEGORY_PROCESSES,
                       IDI_SYSTEM_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryProcesses() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryProcesses);
};

class CategoryLicenses : public CategoryInfo
{
public:
    CategoryLicenses()
        : CategoryInfo(system_info::software::kLicenses,
                       IDS_SI_CATEGORY_LICENSES,
                       IDI_LICENSE_KEY)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryLicenses() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryLicenses);
};

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
