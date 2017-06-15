//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/comboboxex.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/comboboxex.h"

#include <windowsx.h>

namespace aspia {

ComboBoxEx::ComboBoxEx(HWND hwnd)
{
    Attach(hwnd);
}

LRESULT ComboBoxEx::InsertItem(const std::wstring& text,
                               INT_PTR item_index,
                               int image_index,
                               int indent,
                               LPARAM lparam)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask           = CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_INDENT |
                              CBEIF_LPARAM | CBEIF_TEXT;
    item.pszText        = const_cast<LPWSTR>(text.c_str());
    item.iItem          = item_index;
    item.iImage         = image_index;
    item.iSelectedImage = image_index;
    item.iIndent        = indent;
    item.lParam         = lparam;

    return SendMessageW(hwnd(), CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&item));
}

LRESULT ComboBoxEx::AddItem(const std::wstring& text,
                            int image_index,
                            int indent,
                            LPARAM lparam)
{
    return InsertItem(text, -1, image_index, indent, lparam);
}

LRESULT ComboBoxEx::DeleteItem(INT_PTR item_index)
{
    return SendMessageW(hwnd(), CBEM_DELETEITEM, item_index, 0);
}

void ComboBoxEx::DeleteAllItems()
{
    const int count = GetItemCount();

    for (int index = 0; index < count; ++index)
    {
        DeleteItem(index);
    }
}

LPARAM ComboBoxEx::GetItemData(INT_PTR item_index)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask  = CBEIF_LPARAM;
    item.iItem = item_index;

    if (SendMessageW(hwnd(), CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != 0)
    {
        return item.lParam;
    }

    return -1;
}

LRESULT ComboBoxEx::SetItemData(INT_PTR item_index, LPARAM lparam)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask   = CBEIF_LPARAM;
    item.iItem  = item_index;
    item.lParam = lparam;

    return SendMessageW(hwnd(), CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
}

std::wstring ComboBoxEx::GetItemText(INT_PTR item_index)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask = CBEIF_TEXT;
    item.iItem = item_index;

    wchar_t buffer[256] = { 0 };

    item.pszText = buffer;
    item.cchTextMax = _countof(buffer);

    if (SendMessageW(hwnd(), CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != 0)
        return buffer;

    return std::wstring();
}

LRESULT ComboBoxEx::SetItemText(INT_PTR item_index, const std::wstring& text)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask    = CBEIF_TEXT;
    item.iItem   = item_index;
    item.pszText = const_cast<LPWSTR>(text.c_str());

    return SendMessageW(hwnd(), CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
}

HIMAGELIST ComboBoxEx::SetImageList(HIMAGELIST imagelist)
{
    return reinterpret_cast<HIMAGELIST>(SendMessageW(hwnd(),
                                                     CBEM_SETIMAGELIST,
                                                     0,
                                                     reinterpret_cast<LPARAM>(imagelist)));
}

HIMAGELIST ComboBoxEx::GetImageList()
{
    return reinterpret_cast<HIMAGELIST>(SendMessageW(hwnd(), CBEM_GETIMAGELIST, 0, 0));
}

int ComboBoxEx::GetItemCount()
{
    HWND combobox =
        reinterpret_cast<HWND>(SendMessageW(hwnd(), CBEM_GETCOMBOCONTROL, 0, 0));

    return ComboBox_GetCount(combobox);
}

int ComboBoxEx::GetSelectedItem()
{
    return SendMessageW(hwnd(), CB_GETCURSEL, 0, 0);
}

} // namespace aspia
