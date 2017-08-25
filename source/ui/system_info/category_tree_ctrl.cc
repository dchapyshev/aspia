//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_tree_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/version_helpers.h"
#include "ui/system_info/category_tree_ctrl.h"
#include "ui/system_info/category_group_hardware.h"
#include "ui/system_info/category_group_software.h"
#include "ui/system_info/category_group_network.h"
#include "ui/system_info/category_group_os.h"
#include "ui/system_info/category_summary.h"
#include "ui/resource.h"

#include <uxtheme.h>

namespace aspia {

void CategoryTreeCtrl::InitializeCategoryList()
{
    category_list_.emplace_back(std::make_unique<CategorySummary>());
    category_list_.emplace_back(std::make_unique<CategoryGroupHardware>());
    category_list_.emplace_back(std::make_unique<CategoryGroupSoftware>());
    category_list_.emplace_back(std::make_unique<CategoryGroupNetwork>());
    category_list_.emplace_back(std::make_unique<CategoryGroupOS>());
}

void CategoryTreeCtrl::AddChildItems(const CategoryList& list, HTREEITEM parent_tree_item)
{
    for (const auto& child : list)
    {
        int icon_index = imagelist_.AddIcon(child->Icon());

        HTREEITEM tree_item = InsertItem(
            child->Name(), icon_index, icon_index, parent_tree_item, TVI_LAST);

        SetItemData(tree_item, reinterpret_cast<DWORD_PTR>(&child));

        if (child->type() == Category::Type::GROUP)
        {
            AddChildItems(child->child_list(), tree_item);
        }
    }
}

void CategoryTreeCtrl::ExpandCategoryGroups(HTREEITEM parent_tree_item)
{
    HTREEITEM child = GetChildItem(parent_tree_item);

    while (child)
    {
        Expand(child);
        child = GetNextItem(child, TVGN_NEXT);
    }
}

LRESULT CategoryTreeCtrl::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        static const DWORD kDoubleBuffer = 0x0004;
        SetExtendedStyle(kDoubleBuffer, kDoubleBuffer);
    }

    InitializeCategoryList();

    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);
    }

    AddChildItems(category_list_, TVI_ROOT);
    ExpandCategoryGroups(TVI_ROOT);

    return ret;
}

} // namespace aspia
