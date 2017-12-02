//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_select_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_SELECT_DIALOG_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_SELECT_DIALOG_H

#include "base/macros.h"
#include "protocol/category.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class CategorySelectDialog :
    public CDialogImpl<CategorySelectDialog>,
    public CDialogResize<CategorySelectDialog>
{
public:
    enum { IDD = IDD_CATEGORY_SELECT };

    CategorySelectDialog(CategoryList* category_list);
    ~CategorySelectDialog() = default;

    const CategoryList& GetCategoryTree();

private:
    BEGIN_MSG_MAP(CategorySelectDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_SELECT_ALL, OnSelectAllButton)
        COMMAND_ID_HANDLER(IDC_UNSELECT_ALL, OnUnselectAllButton)
        COMMAND_ID_HANDLER(IDOK, OnSaveButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        NOTIFY_CODE_HANDLER(TVN_ITEMCHANGED, OnTreeItemChanged)

        CHAIN_MSG_MAP(CDialogResize<CategorySelectDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(CategorySelectDialog)
        DLGRESIZE_CONTROL(IDC_CATEGORY_TREE, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDC_SELECT_ALL, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDC_UNSELECT_ALL, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSelectAllButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnUnselectAllButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnSaveButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled);

    LRESULT OnTreeItemChanged(int control_id, LPNMHDR hdr, BOOL& handled);

    void AddChildItems(CTreeViewCtrl& treeview,
                       const CSize& icon_size,
                       const CategoryList& tree,
                       HTREEITEM parent_tree_item);
    static void SetCheckStateForItem(CTreeViewCtrl& treeview,
                                     HTREEITEM item,
                                     BOOL state);
    static void SetCheckStateForChildItems(CTreeViewCtrl& treeview,
                                           HTREEITEM parent_item,
                                           BOOL state);

    CategoryList* category_list_;

    CIcon small_icon_;
    CIcon big_icon_;
    CIcon select_all_icon_;
    CIcon unselect_all_icon_;

    CImageListManaged imagelist_;

    bool checkbox_rebuild_ = false;

    DISALLOW_COPY_AND_ASSIGN(CategorySelectDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_SELECT_DIALOG_H
