//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_tree_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H

#include "base/macros.h"
#include "protocol/category.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class CategoryTreeCtrl : public CWindowImpl<CategoryTreeCtrl, CTreeViewCtrl>
{
public:
    CategoryTreeCtrl() = default;
    ~CategoryTreeCtrl() = default;

    enum class ItemType { UNKNOWN, GROUP, CATEGORY };

    ItemType GetItemType(HTREEITEM tree_item) const;
    CategoryInfo* GetItemCategory(HTREEITEM tree_item) const;
    CategoryGroup* GetItemGroup(HTREEITEM tree_item) const;
    void ExpandChildGroups(HTREEITEM parent_tree_item);

private:
    BEGIN_MSG_MAP(CategoryTreeCtrl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    void AddChildItems(const CategoryList& tree, HTREEITEM parent_tree_item);

    CategoryList category_tree_;
    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(CategoryTreeCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_TREE_CTRL_H
