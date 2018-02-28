//
// PROJECT:         Aspia
// FILE:            ui/system_info/category_tree_ctrl.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H

#include "category/category.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class CategoryTreeCtrl : public CWindowImpl<CategoryTreeCtrl, CTreeViewCtrl>
{
public:
    CategoryTreeCtrl() = default;
    ~CategoryTreeCtrl() = default;

    void AddChildItems(const CSize& icon_size,
                       const CategoryList& tree,
                       HTREEITEM parent_tree_item);
    Category* GetItemCategory(HTREEITEM tree_item) const;
    void ExpandChildGroups(HTREEITEM parent_tree_item);

private:
    BEGIN_MSG_MAP(CategoryTreeCtrl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(CategoryTreeCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H
