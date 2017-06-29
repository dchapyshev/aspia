//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/combobox.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__COMBOBOX_H
#define _ASPIA_UI__BASE__COMBOBOX_H

#include "ui/base/window.h"

namespace aspia {

class UiComboBox : public UiWindow
{
public:
    UiComboBox() = default;
    UiComboBox(HWND hwnd);

    int AddItem(const std::wstring& title, LPARAM item_data);
    int AddItem(const WCHAR* title, LPARAM item_data);

    template <typename T>
    int AddItem(const std::wstring& title, T item_data)
    {
        return AddItem(title, static_cast<LPARAM>(item_data));
    }

    template <typename T>
    int AddItem(const WCHAR* title, T item_data)
    {
        return AddItem(title, static_cast<LPARAM>(item_data));
    }

    LPARAM CurItemData();

    void SelectItemWithData(LPARAM item_data);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__COMBOBOX_H
