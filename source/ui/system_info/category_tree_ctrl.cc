//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_tree_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "ui/system_info/category_tree_ctrl.h"
#include "ui/resource.h"

#include <uxtheme.h>

namespace aspia {

void CategoryTreeCtrl::AddChildItems(const CSize& icon_size,
                                     const CategoryList& tree,
                                     HTREEITEM parent_tree_item)
{
    for (const auto& child : tree)
    {
        const int icon_index =
            imagelist_.AddIcon(AtlLoadIconImage(child->Icon(),
                                                LR_CREATEDIBSECTION,
                                                icon_size.cx,
                                                icon_size.cy));

        HTREEITEM tree_item = InsertItem(
            UNICODEfromUTF8(child->Name()).c_str(),
            icon_index,
            icon_index,
            parent_tree_item,
            TVI_LAST);

        // Each element in the tree contains a pointer to the category.
        SetItemData(tree_item, reinterpret_cast<DWORD_PTR>(child.get()));

        if (child->type() == Category::Type::GROUP)
        {
            AddChildItems(icon_size, child->category_group()->child_list(), tree_item);
        }
    }
}

void CategoryTreeCtrl::ExpandChildGroups(HTREEITEM parent_tree_item)
{
    HTREEITEM child = GetChildItem(parent_tree_item);

    while (child)
    {
        Category* category = GetItemCategory(child);

        if (category && category->type() == Category::Type::GROUP)
        {
            Expand(child);
        }

        child = GetNextItem(child, TVGN_NEXT);
    }
}

LRESULT CategoryTreeCtrl::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(handled);

    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        static const DWORD kDoubleBuffer = 0x0004;
        SetExtendedStyle(kDoubleBuffer, kDoubleBuffer);
    }

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx,
                          small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);
    }

    return ret;
}

Category* CategoryTreeCtrl::GetItemCategory(HTREEITEM tree_item) const
{
    return reinterpret_cast<Category*>(GetItemData(tree_item));
}

} // namespace aspia
