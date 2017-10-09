//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_windows_devices.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_windows_devices.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryAllDevices : public CategoryInfo
{
public:
    CategoryAllDevices()
        : CategoryInfo(system_info::hardware::windows_devices::kAll,
                       IDS_SI_CATEGORY_WINDOWS_DEVICES_ALL,
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryAllDevices() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryAllDevices);
};

class CategoryUnknownDevices : public CategoryInfo
{
public:
    CategoryUnknownDevices()
        : CategoryInfo(system_info::hardware::windows_devices::kUnknown,
                       IDS_SI_CATEGORY_WINDOWS_DEVICES_UNK,
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryUnknownDevices() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUnknownDevices);
};

CategoryGroupWindowDevices::CategoryGroupWindowDevices()
    : CategoryGroup(IDS_SI_CATEGORY_WINDOWS_DEVICES, IDI_PCI)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryAllDevices>());
    child_list->emplace_back(std::make_unique<CategoryUnknownDevices>());
}

} // namespace aspia
