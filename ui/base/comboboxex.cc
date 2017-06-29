//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/comboboxex.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/comboboxex.h"

namespace aspia {

UiComboBoxEx::UiComboBoxEx(HWND hwnd)
{
    Attach(hwnd);
}

bool UiComboBoxEx::Create(HWND parent, int ctrl_id, DWORD style, HINSTANCE instance)
{
    Attach(CreateWindowExW(0,
                           WC_COMBOBOXEXW,
                           L"",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                           0, 0, 200, 200,
                           parent,
                           reinterpret_cast<HMENU>(ctrl_id),
                           instance,
                           nullptr));
    if (hwnd() == nullptr)
        return false;

    return true;
}

int UiComboBoxEx::InsertItem(const WCHAR* text,
                             int item_index,
                             int image_index,
                             int indent,
                             LPARAM lparam)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask = CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_INDENT |
        CBEIF_LPARAM | CBEIF_TEXT;
    item.pszText        = const_cast<LPWSTR>(text);
    item.iItem          = item_index;
    item.iImage         = image_index;
    item.iSelectedImage = image_index;
    item.iIndent        = indent;
    item.lParam         = lparam;

    return SendMessageW(CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&item));
}

int UiComboBoxEx::InsertItem(const std::wstring& text,
                             int item_index,
                             int image_index,
                             int indent,
                             LPARAM lparam)
{
    return InsertItem(text.c_str(), item_index, image_index, indent, lparam);
}

int UiComboBoxEx::AddItem(const WCHAR* text,
                          int image_index,
                          int indent,
                          LPARAM lparam)
{
    return InsertItem(text, -1, image_index, indent, lparam);
}

int UiComboBoxEx::AddItem(const std::wstring& text,
                          int image_index,
                          int indent,
                          LPARAM lparam)
{
    return InsertItem(text, -1, image_index, indent, lparam);
}

int UiComboBoxEx::DeleteItem(int item_index)
{
    return SendMessageW(CBEM_DELETEITEM, item_index, 0);
}

int UiComboBoxEx::DeleteItemWithData(LPARAM lparam)
{
    int item_index = GetItemWithData(lparam);

    if (item_index != CB_ERR)
        return DeleteItem(item_index);

    return CB_ERR;
}

void UiComboBoxEx::DeleteAllItems()
{
    SendMessageW(CB_RESETCONTENT, 0, 0);
}

LPARAM UiComboBoxEx::GetItemData(int item_index)
{
    COMBOBOXEXITEMW item;
    memset(&item, 0, sizeof(item));

    item.mask  = CBEIF_LPARAM;
    item.iItem = item_index;

    if (SendMessageW(CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != 0)
    {
        return item.lParam;
    }

    return -1;
}

LRESULT UiComboBoxEx::SetItemData(int item_index, LPARAM lparam)
{
    COMBOBOXEXITEMW item;

    memset(&item, 0, sizeof(item));

    item.mask   = CBEIF_LPARAM;
    item.iItem  = item_index;
    item.lParam = lparam;

    return SendMessageW(CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
}

std::wstring UiComboBoxEx::GetItemText(int item_index)
{
    COMBOBOXEXITEMW item;
    memset(&item, 0, sizeof(item));

    item.mask = CBEIF_TEXT;
    item.iItem = item_index;

    wchar_t buffer[256] = { 0 };

    item.pszText = buffer;
    item.cchTextMax = _countof(buffer);

    if (SendMessageW(CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != 0)
        return buffer;

    return std::wstring();
}

int UiComboBoxEx::GetItemImage(int item_index)
{
    COMBOBOXEXITEMW item;
    memset(&item, 0, sizeof(item));

    item.mask = CBEIF_IMAGE;
    item.iItem = item_index;

    if (SendMessageW(CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != 0)
    {
        return item.iImage;
    }

    return -1;
}

LRESULT UiComboBoxEx::SetItemText(int item_index, const std::wstring& text)
{
    COMBOBOXEXITEMW item;
    memset(&item, 0, sizeof(item));

    item.mask    = CBEIF_TEXT;
    item.iItem   = item_index;
    item.pszText = const_cast<LPWSTR>(text.c_str());

    return SendMessageW(CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
}

HIMAGELIST UiComboBoxEx::SetImageList(HIMAGELIST imagelist)
{
    return reinterpret_cast<HIMAGELIST>(SendMessageW(CBEM_SETIMAGELIST,
                                                     0,
                                                     reinterpret_cast<LPARAM>(imagelist)));
}

HIMAGELIST UiComboBoxEx::GetImageList()
{
    return reinterpret_cast<HIMAGELIST>(SendMessageW(CBEM_GETIMAGELIST, 0, 0));
}

int UiComboBoxEx::GetItemCount()
{
    return SendMessageW(CB_GETCOUNT, 0, 0);
}

int UiComboBoxEx::GetSelectedItem()
{
    return SendMessageW(CB_GETCURSEL, 0, 0);
}

int UiComboBoxEx::GetItemWithData(LPARAM lparam)
{
    int count = GetItemCount();

    for (int index = 0; index < count; ++index)
    {
        if (GetItemData(index) == lparam)
            return index;
    }

    return CB_ERR;
}

void UiComboBoxEx::SelectItem(int item_index)
{
    SendMessageW(CB_SETCURSEL, item_index, 0);
}

void UiComboBoxEx::SelectItemData(LPARAM lparam)
{
    int item_index = GetItemWithData(lparam);

    if (item_index != CB_ERR)
        SelectItem(item_index);
}

} // namespace aspia
