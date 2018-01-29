//
// PROJECT:         Aspia
// FILE:            ui/computer_group_tree_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/computer_group_tree_ctrl.h"

#include <functional>

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const int kComputerGroupIcon = 0;

} // namespace

bool ComputerGroupTreeCtrl::Create(HWND parent, UINT control_id)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES |
        TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_LINESATROOT;

    if (!CWindowImpl<ComputerGroupTreeCtrl, CTreeViewCtrl>::Create(
            parent, rcDefault, nullptr, style, WS_EX_CLIENTEDGE, control_id))
    {
        return false;
    }

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        static const DWORD kDoubleBuffer = 0x0004;
        SetExtendedStyle(kDoubleBuffer, kDoubleBuffer);
    }

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx, small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        CIcon icon = AtlLoadIconImage(IDI_FOLDER,
                                      LR_CREATEDIBSECTION,
                                      small_icon_size.cx,
                                      small_icon_size.cy);
        imagelist_.AddIcon(icon);
        SetImageList(imagelist_);
    }

    return true;
}

HTREEITEM ComputerGroupTreeCtrl::AddComputerGroup(HTREEITEM parent_item,
                                                  proto::ComputerGroup* computer_group)
{
    HTREEITEM item = InsertItem(UNICODEfromUTF8(computer_group->name()).c_str(),
                                kComputerGroupIcon,
                                kComputerGroupIcon,
                                parent_item,
                                TVI_LAST);
    if (item)
    {
        SetItemData(item, reinterpret_cast<DWORD_PTR>(computer_group));
    }

    return item;
}

HTREEITEM ComputerGroupTreeCtrl::AddComputerGroupTree(
    HTREEITEM item, proto::ComputerGroup* computer_group)
{
    std::function<void(HTREEITEM, proto::ComputerGroup*)> add_child_items =
        [&](HTREEITEM parent_item, proto::ComputerGroup* parent_group)
    {
        for (int i = 0; i < parent_group->group_size(); ++i)
        {
            proto::ComputerGroup* child_group = parent_group->mutable_group(i);

            HTREEITEM item = AddComputerGroup(parent_item, child_group);

            if (item)
            {
                add_child_items(item, child_group);

                if (child_group->expanded())
                    Expand(item, TVE_EXPAND);
            }
        }
    };

    HTREEITEM root = AddComputerGroup(item, computer_group);
    if (root)
        add_child_items(root, computer_group);

    return root;
}

proto::ComputerGroup* ComputerGroupTreeCtrl::GetComputerGroup(HTREEITEM item)
{
    return reinterpret_cast<proto::ComputerGroup*>(GetItemData(item));
}

void ComputerGroupTreeCtrl::UpdateComputerGroup(
    HTREEITEM item, proto::ComputerGroup* computer_group)
{
    SetItemData(item, reinterpret_cast<DWORD_PTR>(computer_group));
    SetItemText(item, UNICODEfromUTF8(computer_group->name()).c_str());
}

bool ComputerGroupTreeCtrl::IsItemContainsChild(HTREEITEM item, HTREEITEM child_item)
{
    std::function<bool(HTREEITEM _item)> is_contains = [&](HTREEITEM _item)
    {
        for (HTREEITEM current = GetChildItem(_item);
             current != nullptr;
             current = GetNextSiblingItem(current))
        {
            if (current == child_item)
                return true;

            return is_contains(current);
        }

        return false;
    };

    return is_contains(item);
}

} // namespace aspia
