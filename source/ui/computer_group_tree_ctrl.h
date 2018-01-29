//
// PROJECT:         Aspia
// FILE:            ui/computer_group_tree_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__COMPUTER_GROUP_TREE_CTRL_H
#define _ASPIA_UI__COMPUTER_GROUP_TREE_CTRL_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

#include "proto/address_book.pb.h"

namespace aspia {

class ComputerGroupTreeCtrl : public CWindowImpl<ComputerGroupTreeCtrl, CTreeViewCtrl>
{
public:
    ComputerGroupTreeCtrl() = default;

    bool Create(HWND parent, UINT control_id);

    HTREEITEM AddComputerGroup(HTREEITEM parent_item, proto::ComputerGroup* computer_group);
    HTREEITEM AddComputerGroupTree(HTREEITEM parent_item, proto::ComputerGroup* computer_group);
    proto::ComputerGroup* GetComputerGroup(HTREEITEM item);
    void UpdateComputerGroup(HTREEITEM item, proto::ComputerGroup* computer_group);
    bool IsItemContainsChild(HTREEITEM item, HTREEITEM child_item);

private:
    BEGIN_MSG_MAP(ComputerGroupTreeCtrl)
        // Nothing
    END_MSG_MAP()

    CImageListManaged imagelist_;
};

} // namespace aspia

#endif // _ASPIA_UI__COMPUTER_GROUP_TREE_CTRL_H
