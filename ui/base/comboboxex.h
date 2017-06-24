//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/comboboxex.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__COMBOBOXEX_H
#define _ASPIA_UI__BASE__COMBOBOXEX_H

#include "ui/base/window.h"
#include "ui/base/imagelist.h"

#include <commctrl.h>
#include <string>

namespace aspia {

class UiComboBoxEx : public UiWindow
{
public:
    UiComboBoxEx() = default;
    UiComboBoxEx(HWND hwnd);

    bool Create(HWND parent, int ctrl_id, DWORD style, HINSTANCE instance);

    int InsertItem(const std::wstring& text,
                   int item_index,
                   int image_index,
                   int indent,
                   LPARAM lparam);

    int AddItem(const std::wstring& text,
                int image_index,
                int indent,
                LPARAM lparam);

    int DeleteItem(int item_index);
    int DeleteItemWithData(LPARAM lparam);
    void DeleteAllItems();

    LPARAM GetItemData(int item_index);
    LRESULT SetItemData(int item_index, LPARAM lparam);

    std::wstring GetItemText(int item_index);
    LRESULT SetItemText(int item_index, const std::wstring& text);

    int GetItemImage(int item_index);

    HIMAGELIST SetImageList(HIMAGELIST imagelist);
    HIMAGELIST GetImageList();

    int GetItemCount();
    int GetSelectedItem();
    int GetItemWithData(LPARAM lparam);

    void SelectItem(int item_index);
    void SelectItemData(LPARAM lparam);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__COMBOBOXEX_H
