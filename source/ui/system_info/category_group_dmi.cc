//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_dmi.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_dmi.h"

namespace aspia {

class CategoryDMI : public Category
{
public:
    CategoryDMI(UINT name_id, UINT icon_id);
    ~CategoryDMI() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDMI);
};

CategoryDMI::CategoryDMI(UINT name_id, UINT icon_id)
    : Category(Type::REGULAR, name_id, icon_id)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

CategoryGroupDMI::CategoryGroupDMI()
    : CategoryGroup(IDS_SI_CATEGORY_DMI, IDI_COMPUTER)
{
    CategoryList* child_list = mutable_child_list();

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_BIOS, IDI_BIOS));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_SYSTEM, IDI_COMPUTER));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_MOTHERBOARD, IDI_MOTHERBOARD));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_CHASSIS, IDI_SERVER));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_CACHES, IDI_CHIP));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_PROCESSORS, IDI_PROCESSOR));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_MEMORY_DEVICES, IDI_MEMORY));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_SYSTEM_SLOTS, IDI_PORT));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_PORT_CONNECTORS, IDI_PORT));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_ONBOARD_DEVICES, IDI_MOTHERBOARD));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_BUILDIN_POINTING, IDI_MOUSE));

    child_list->emplace_back(
        std::make_unique<CategoryDMI>(IDS_SI_CATEGORY_DMI_PORTABLE_BATTERY, IDI_BATTERY));
}

} // namespace aspia
