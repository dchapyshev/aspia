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

class ComboBoxEx : public Window
{
public:
    ComboBoxEx() = default;
    ComboBoxEx(HWND hwnd);

    LRESULT InsertItem(const std::wstring& text,
                       INT_PTR item_index,
                       int image_index,
                       int indent,
                       LPARAM lparam);

    LRESULT AddItem(const std::wstring& text,
                    int image_index,
                    int indent,
                    LPARAM lparam);

    LRESULT DeleteItem(INT_PTR item_index);

    LPARAM GetItemData(INT_PTR item_index);
    LRESULT SetItemData(INT_PTR item_index, LPARAM lparam);

    std::wstring GetItemText(INT_PTR item_index);
    LRESULT SetItemText(INT_PTR item_index, const std::wstring& text);

    HIMAGELIST SetImageList(HIMAGELIST imagelist);
    HIMAGELIST GetImageList();

    int GetItemCount();
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__COMBOBOXEX_H
