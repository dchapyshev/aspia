//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/combobox.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__COMBOBOX_H
#define _ASPIA_UI__BASE__COMBOBOX_H

#include <windowsx.h>

namespace aspia {

template <typename T>
static INLINE int
ComboBox_AddItem(HWND combobox, const std::wstring& title, T item_data)
{
    int item_index = ComboBox_AddString(combobox, title.c_str());

    if (item_index == CB_ERR)
        return CB_ERR;

    ComboBox_SetItemData(combobox, item_index, static_cast<LPARAM>(item_data));

    return item_index;
}

static INLINE LPARAM
ComboBox_CurItemData(HWND combobox)
{
    int item_index = ComboBox_GetCurSel(combobox);

    if (item_index == CB_ERR)
        return CB_ERR;

    return ComboBox_GetItemData(combobox, item_index);
}

template <typename T>
static INLINE void
ComboBox_SelItemWithData(HWND combobox, T item_data)
{
    int count = ComboBox_GetCount(combobox);

    for (int i = 0; i < count; ++i)
    {
        LPARAM cur_item_data = ComboBox_GetItemData(combobox, i);

        if (cur_item_data == static_cast<LPARAM>(item_data))
        {
            ComboBox_SetCurSel(combobox, i);
            return;
        }
    }
}

} // namespace aspia

#endif // _ASPIA_UI__BASE__COMBOBOX_H
