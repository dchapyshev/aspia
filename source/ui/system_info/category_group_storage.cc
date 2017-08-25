//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_storage.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/category_group_storage.h"

namespace aspia {

//
// Optical Drives
//

class CategoryOpticalDrives : public Category
{
public:
    CategoryOpticalDrives();
    ~CategoryOpticalDrives() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpticalDrives);
};

CategoryOpticalDrives::CategoryOpticalDrives()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_STORAGE_OPTICAL_DRIVES, IDI_DRIVE_DISK)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// ATA
//

class CategoryATA : public Category
{
public:
    CategoryATA();
    ~CategoryATA() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryATA);
};

CategoryATA::CategoryATA()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_STORAGE_ATA, IDI_DRIVE)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// S.M.A.R.T.
//

class CategorySMART : public Category
{
public:
    CategorySMART();
    ~CategorySMART() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySMART);
};

CategorySMART::CategorySMART()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_STORAGE_SMART, IDI_DRIVE)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Storage Group
//

CategoryGroupStorage::CategoryGroupStorage()
    : CategoryGroup(IDS_SI_CATEGORY_STORAGE, IDI_DRIVE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryOpticalDrives>());
    child_list->emplace_back(std::make_unique<CategoryATA>());
    child_list->emplace_back(std::make_unique<CategorySMART>());
}

} // namespace aspia
