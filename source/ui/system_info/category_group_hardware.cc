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

namespace aspia {

//
// Central Processor
//

class CategoryCPU : public Category
{
public:
    CategoryCPU();
    ~CategoryCPU() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryCPU);
};

CategoryCPU::CategoryCPU()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_CPU, IDI_PROCESSOR)
{
    set_guid(system_info::hardware::kCPU);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Printers
//

class CategoryPrinters : public Category
{
public:
    CategoryPrinters();
    ~CategoryPrinters() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrinters);
};

CategoryPrinters::CategoryPrinters()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_PRINTERS, IDI_PRINTER)
{
    set_guid(system_info::hardware::kPrinters);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Power Options
//

class CategoryPowerOptions : public Category
{
public:
    CategoryPowerOptions();
    ~CategoryPowerOptions() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPowerOptions);
};

CategoryPowerOptions::CategoryPowerOptions()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_POWER_OPTIONS, IDI_POWER_SUPPLY)
{
    set_guid(system_info::hardware::kPowerOptions);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Hardware Group
//

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
