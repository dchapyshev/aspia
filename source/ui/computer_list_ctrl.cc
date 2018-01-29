//
// PROJECT:         Aspia
// FILE:            ui/computer_list_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/computer_list_ctrl.h"

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const int kComputerIcon = 0;

} // namespace

bool ComputerListCtrl::Create(HWND parent, UINT control_id)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_TABSTOP | LVS_SHOWSELALWAYS;

    if (!CWindowImpl<ComputerListCtrl, CListViewCtrl>::Create(
            parent, rcDefault, nullptr, style, WS_EX_CLIENTEDGE, control_id))
    {
        return false;
    }

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    SetExtendedListViewStyle(ex_style);

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx, small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        CIcon icon = AtlLoadIconImage(IDI_COMPUTER,
                                               LR_CREATEDIBSECTION,
                                               small_icon_size.cx,
                                               small_icon_size.cy);
        imagelist_.AddIcon(icon);

        SetImageList(imagelist_, LVSIL_SMALL);

        auto add_column = [&](UINT resource_id, int column_index, int width)
        {
            CString text;
            text.LoadStringW(resource_id);

            AddColumn(text, column_index);
            SetColumnWidth(column_index, width);
        };

        add_column(IDS_COL_NAME, 0, 250);
        add_column(IDS_COL_ADDRESS, 1, 200);
        add_column(IDS_COL_PORT, 2, 100);
    }

    return true;
}

int ComputerListCtrl::AddComputer(proto::Computer* computer)
{
    DCHECK(computer);

    int item_index = AddItem(
        GetItemCount(), 0, UNICODEfromUTF8(computer->name()).c_str(), kComputerIcon);

    SetItemData(item_index, reinterpret_cast<DWORD_PTR>(computer));
    SetItemText(item_index, 1, UNICODEfromUTF8(computer->address()).c_str());
    SetItemText(item_index, 2, std::to_wstring(computer->port()).c_str());

    return item_index;
}

void ComputerListCtrl::AddChildComputers(proto::ComputerGroup* computer_group)
{
    DCHECK(computer_group);

    for (int i = 0; i < computer_group->computer_size(); ++i)
    {
        AddComputer(computer_group->mutable_computer(i));
    }
}

proto::Computer* ComputerListCtrl::GetComputer(int item_index)
{
    if (item_index == -1)
        return nullptr;

    return reinterpret_cast<proto::Computer*>(GetItemData(item_index));
}

void ComputerListCtrl::UpdateComputer(int item_index, proto::Computer* computer)
{
    DCHECK(computer);

    if (item_index == -1)
        return;

    SetItemData(item_index, reinterpret_cast<DWORD_PTR>(computer));
    SetItemText(item_index, 0, UNICODEfromUTF8(computer->name()).c_str());
    SetItemText(item_index, 1, UNICODEfromUTF8(computer->address()).c_str());
    SetItemText(item_index, 2, std::to_wstring(computer->port()).c_str());
}

} // namespace aspia
