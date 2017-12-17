//
// PROJECT:         Aspia
// FILE:            ui/system_info/category_select_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "ui/system_info/category_select_dialog.h"

#include <uxtheme.h>

namespace aspia {

#define STATEIMAGEMASKTOINDEX(state) (((state) & TVIS_STATEIMAGEMASK) >> 12)

CategorySelectDialog::CategorySelectDialog(CategoryList* category_list)
    : category_list_(category_list)
{
    DCHECK(category_list_);
}

void CategorySelectDialog::AddChildItems(CTreeViewCtrl& treeview,
                                         const CSize& icon_size,
                                         const CategoryList& tree,
                                         HTREEITEM parent_tree_item)
{
    for (const auto& child : tree)
    {
        CIcon icon = AtlLoadIconImage(child->Icon(),
                                      LR_CREATEDIBSECTION,
                                      icon_size.cx,
                                      icon_size.cy);
        const int icon_index = imagelist_.AddIcon(icon);

        HTREEITEM tree_item = treeview.InsertItem(
            UNICODEfromUTF8(child->Name()).c_str(),
            icon_index,
            icon_index,
            parent_tree_item,
            TVI_LAST);

        // Each element in the tree contains a pointer to the category.
        treeview.SetItemData(tree_item, reinterpret_cast<DWORD_PTR>(child.get()));

        if (child->type() == Category::Type::GROUP)
        {
            AddChildItems(treeview, icon_size, child->category_group()->child_list(), tree_item);
        }
    }
}

LRESULT CategorySelectDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();
    CenterWindow();

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   small_icon_size.cx,
                                   small_icon_size.cy);
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    select_all_icon_ = AtlLoadIconImage(IDI_SELECT_ALL,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy);
    CButton(GetDlgItem(IDC_SELECT_ALL)).SetIcon(select_all_icon_);

    unselect_all_icon_ = AtlLoadIconImage(IDI_UNSELECT_ALL,
                                          LR_CREATEDIBSECTION,
                                          small_icon_size.cx,
                                          small_icon_size.cy);
    CButton(GetDlgItem(IDC_UNSELECT_ALL)).SetIcon(unselect_all_icon_);

    expand_all_icon_ = AtlLoadIconImage(IDI_TREE,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy);
    CButton(GetDlgItem(IDC_EXPAND_ALL)).SetIcon(expand_all_icon_);

    collapse_all_icon_ = AtlLoadIconImage(IDI_LIST,
                                          LR_CREATEDIBSECTION,
                                          small_icon_size.cx,
                                          small_icon_size.cy);
    CButton(GetDlgItem(IDC_COLLAPSE_ALL)).SetIcon(collapse_all_icon_);

    CTreeViewCtrl treeview(GetDlgItem(IDC_CATEGORY_TREE));

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(treeview, L"explorer", nullptr);
        static const DWORD kDoubleBuffer = 0x0004;
        treeview.SetExtendedStyle(kDoubleBuffer, kDoubleBuffer);
    }

    if (imagelist_.Create(small_icon_size.cx,
                          small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        treeview.SetImageList(imagelist_);
    }

    treeview.ModifyStyle(0, TVS_CHECKBOXES);

    AddChildItems(treeview, small_icon_size, *category_list_, TVI_ROOT);

    return FALSE;
}

LRESULT CategorySelectDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT CategorySelectDialog::OnSelectAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CTreeViewCtrl treeview(GetDlgItem(IDC_CATEGORY_TREE));
    SetCheckStateForChildItems(treeview, TVI_ROOT, TRUE);
    return 0;
}

LRESULT CategorySelectDialog::OnUnselectAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CTreeViewCtrl treeview(GetDlgItem(IDC_CATEGORY_TREE));
    SetCheckStateForChildItems(treeview, TVI_ROOT, FALSE);
    return 0;
}

LRESULT CategorySelectDialog::OnExpandAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CTreeViewCtrl treeview(GetDlgItem(IDC_CATEGORY_TREE));
    ExpandChildItems(treeview, TVI_ROOT, TVE_EXPAND);
    return 0;
}

LRESULT CategorySelectDialog::OnCollapseAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CTreeViewCtrl treeview(GetDlgItem(IDC_CATEGORY_TREE));
    ExpandChildItems(treeview, TVI_ROOT, TVE_COLLAPSE);
    return 0;
}

LRESULT CategorySelectDialog::OnSaveButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDOK);
    return 0;
}

LRESULT CategorySelectDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

// static
void CategorySelectDialog::SetCheckStateForItem(
    CTreeViewCtrl& treeview, HTREEITEM item, BOOL state)
{
    Category* category = reinterpret_cast<Category*>(treeview.GetItemData(item));

    if (category)
    {
        treeview.SetCheckState(item, state);

        if (category->type() == Category::Type::INFO_LIST ||
            category->type() == Category::Type::INFO_PARAM_VALUE)
        {
            category->category_info()->SetChecked(!!state);
        }
    }
}

// static
void CategorySelectDialog::SetCheckStateForChildItems(
    CTreeViewCtrl& treeview, HTREEITEM parent_item, BOOL state)
{
    for (HTREEITEM item = treeview.GetChildItem(parent_item);
         item != nullptr;
         item = treeview.GetNextSiblingItem(item))
    {
        SetCheckStateForItem(treeview, item, state);
        SetCheckStateForChildItems(treeview, item, state);
    }
}

// static
void CategorySelectDialog::ExpandChildItems(
    CTreeViewCtrl& treeview, HTREEITEM parent_item, UINT flag)
{
    for (HTREEITEM item = treeview.GetChildItem(parent_item);
         item != nullptr;
         item = treeview.GetNextSiblingItem(item))
    {
        Category* category = reinterpret_cast<Category*>(treeview.GetItemData(item));

        if (category && category->type() == Category::Type::GROUP)
        {
            treeview.Expand(item, flag);
            ExpandChildItems(treeview, item, flag);
        }
    }
}

LRESULT CategorySelectDialog::OnTreeItemChanged(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    if (checkbox_rebuild_)
        return FALSE;

    checkbox_rebuild_ = true;

    NMTVITEMCHANGE* item_change = reinterpret_cast<NMTVITEMCHANGE*>(hdr);

    if (item_change->uChanged == TVIF_STATE && item_change->lParam)
    {
        BOOL old_state = static_cast<BOOL>((STATEIMAGEMASKTOINDEX(item_change->uStateOld) - 1));
        BOOL new_state = static_cast<BOOL>((STATEIMAGEMASKTOINDEX(item_change->uStateNew) - 1));

        // If the checkbox state has changed.
        if (old_state != new_state)
        {
            CTreeViewCtrl treeview(item_change->hdr.hwndFrom);

            SetCheckStateForItem(treeview, item_change->hItem, new_state);
            SetCheckStateForChildItems(treeview, item_change->hItem, new_state);

            HTREEITEM parent_item = treeview.GetParentItem(item_change->hItem);

            while (parent_item)
            {
                if (new_state)
                {
                    treeview.SetCheckState(parent_item, TRUE);
                }
                else
                {
                    bool has_checked_child_items = false;

                    for (HTREEITEM item = treeview.GetChildItem(parent_item);
                         item != nullptr;
                         item = treeview.GetNextSiblingItem(item))
                    {
                        if (treeview.GetCheckState(item))
                        {
                            has_checked_child_items = true;
                            break;
                        }
                    }

                    if (!has_checked_child_items)
                        treeview.SetCheckState(parent_item, FALSE);
                }

                parent_item = treeview.GetParentItem(parent_item);
            }
        }
    }

    checkbox_rebuild_ = false;
    return FALSE;
}

} // namespace aspia
