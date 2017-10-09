//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_storage.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_storage.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryOpticalDrives : public CategoryInfo
{
public:
    CategoryOpticalDrives()
        : CategoryInfo(system_info::hardware::storage::kOpticalDrives,
                       IDS_SI_CATEGORY_STORAGE_OPTICAL_DRIVES,
                       IDI_DRIVE_DISK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpticalDrives() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpticalDrives);
};

class CategoryATA : public CategoryInfo
{
public:
    CategoryATA()
        : CategoryInfo(system_info::hardware::storage::kATA,
                       IDS_SI_CATEGORY_STORAGE_ATA,
                       IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryATA() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryATA);
};

class CategorySMART : public CategoryInfo
{
public:
    CategorySMART()
        : CategoryInfo(system_info::hardware::storage::kSMART,
                       IDS_SI_CATEGORY_STORAGE_SMART,
                       IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategorySMART() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySMART);
};

CategoryGroupStorage::CategoryGroupStorage()
    : CategoryGroup(IDS_SI_CATEGORY_STORAGE, IDI_DRIVE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryOpticalDrives>());
    child_list->emplace_back(std::make_unique<CategoryATA>());
    child_list->emplace_back(std::make_unique<CategorySMART>());
}

} // namespace aspia
