//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_hardware.h"
#include "ui/system_info/category_group_dmi.h"
#include "ui/system_info/category_group_storage.h"
#include "ui/system_info/category_group_display.h"
#include "ui/system_info/category_group_windows_devices.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryCPU : public CategoryInfo
{
public:
    CategoryCPU()
        : CategoryInfo(system_info::hardware::kCPU,
                       IDS_SI_CATEGORY_CPU,
                       IDI_PROCESSOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryCPU() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryCPU);
};

class CategoryPrinters : public CategoryInfo
{
public:
    CategoryPrinters()
        : CategoryInfo(system_info::hardware::kPrinters,
                       IDS_SI_CATEGORY_PRINTERS,
                       IDI_PRINTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryPrinters() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrinters);
};

class CategoryPowerOptions : public CategoryInfo
{
public:
    CategoryPowerOptions()
        : CategoryInfo(system_info::hardware::kPowerOptions,
                       IDS_SI_CATEGORY_POWER_OPTIONS,
                       IDI_POWER_SUPPLY)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryPowerOptions() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPowerOptions);
};

CategoryGroupHardware::CategoryGroupHardware()
    : CategoryGroup(IDS_SI_CATEGORY_HARDWARE, IDI_HARDWARE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryGroupDMI>());
    child_list->emplace_back(std::make_unique<CategoryCPU>());
    child_list->emplace_back(std::make_unique<CategoryGroupStorage>());
    child_list->emplace_back(std::make_unique<CategoryGroupDisplay>());
    child_list->emplace_back(std::make_unique<CategoryPowerOptions>());
    child_list->emplace_back(std::make_unique<CategoryPrinters>());
    child_list->emplace_back(std::make_unique<CategoryGroupWindowDevices>());
}

} // namespace aspia
