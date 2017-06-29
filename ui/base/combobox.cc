//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/combobox.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/combobox.h"

#include <windowsx.h>

namespace aspia {

UiComboBox::UiComboBox(HWND hwnd)
{
    Attach(hwnd);
}

int UiComboBox::AddItem(const std::wstring& title, LPARAM item_data)
{
    return AddItem(title.c_str(), item_data);
}

int UiComboBox::AddItem(const WCHAR* title, LPARAM item_data)
{
    int item_index = ComboBox_AddString(hwnd(), title);

    if (item_index == CB_ERR)
        return CB_ERR;

    ComboBox_SetItemData(hwnd(), item_index, item_data);

    return item_index;
}

LPARAM UiComboBox::CurItemData()
{
    int item_index = ComboBox_GetCurSel(hwnd());

    if (item_index == CB_ERR)
        return CB_ERR;

    return ComboBox_GetItemData(hwnd(), item_index);
}

void UiComboBox::SelectItemWithData(LPARAM item_data)
{
    int count = ComboBox_GetCount(hwnd());

    for (int i = 0; i < count; ++i)
    {
        LPARAM cur_item_data = ComboBox_GetItemData(hwnd(), i);

        if (cur_item_data == item_data)
        {
            ComboBox_SetCurSel(hwnd(), i);
            return;
        }
    }
}

} // namespace aspia
