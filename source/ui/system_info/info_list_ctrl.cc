//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/info_list_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/version_helpers.h"
#include "ui/system_info/info_list_ctrl.h"

#include <uxtheme.h>

namespace aspia {

LRESULT InfoListCtrl::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    SetExtendedListViewStyle(ex_style);

    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_, LVSIL_SMALL);
    }

    return ret;
}

void InfoListCtrl::DeleteAllColumns()
{
    int count = GetColumnCount();

    while (--count >= 0)
        DeleteColumn(count);
}

int InfoListCtrl::GetColumnCount() const
{
    CHeaderCtrl header(GetHeader());

    if (!header)
        return 0;

    return header.GetItemCount();
}

} // namespace aspia
