//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_windows_devices.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_windows_devices.h"

namespace aspia {

//
// All Devices
//

class CategoryAllDevices : public Category
{
public:
    CategoryAllDevices();
    ~CategoryAllDevices() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryAllDevices);
};

CategoryAllDevices::CategoryAllDevices()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_WINDOWS_DEVICES_ALL, IDI_PCI)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Unknown Devices
//

class CategoryUnknownDevices : public Category
{
public:
    CategoryUnknownDevices();
    ~CategoryUnknownDevices() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUnknownDevices);
};

CategoryUnknownDevices::CategoryUnknownDevices()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_WINDOWS_DEVICES_UNK, IDI_PCI)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Windows Devices Group
//

CategoryGroupWindowDevices::CategoryGroupWindowDevices()
    : CategoryGroup(IDS_SI_CATEGORY_WINDOWS_DEVICES, IDI_PCI)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryAllDevices>());
    child_list->emplace_back(std::make_unique<CategoryUnknownDevices>());
}

} // namespace aspia
