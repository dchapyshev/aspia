//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/listview.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/listview.h"
#include "ui/base/edit.h"
#include "base/version_helpers.h"

#include <uxtheme.h>

namespace aspia {

UiListView::UiListView(HWND hwnd)
{
    Attach(hwnd);

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(hwnd, L"explorer", nullptr);
        ModifyExtendedListViewStyle(0, LVS_EX_DOUBLEBUFFER);
    }
}

bool UiListView::Create(HWND parent, DWORD ex_style, DWORD style, HINSTANCE instance)
{
    Attach(CreateWindowExW(ex_style,
                           WC_LISTVIEWW,
                           L"",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           parent,
                           nullptr,
                           instance,
                           nullptr));

    if (hwnd())
    {
        if (IsWindowsVistaOrGreater())
        {
            SetWindowTheme(hwnd(), L"explorer", nullptr);
            ModifyExtendedListViewStyle(0, LVS_EX_DOUBLEBUFFER);
        }

        return true;
    }

    return false;
}

int UiListView::GetColumnCount()
{
    HWND header_window = ListView_GetHeader(hwnd());

    if (!header_window)
        return 0;

    return Header_GetItemCount(header_window);
}

int UiListView::GetItemCount()
{
    return ListView_GetItemCount(hwnd());
}

void UiListView::AddOnlyOneColumn(const std::wstring& title)
{
    RECT list_rect = { 0 };
    GetClientRect(hwnd(), &list_rect);

    LVCOLUMNW column = { 0 };

    column.mask    = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    column.cx      = (list_rect.right - list_rect.left) - GetSystemMetrics(SM_CXVSCROLL);
    column.fmt     = LVCFMT_LEFT;
    column.pszText = const_cast<LPWSTR>(title.c_str());

    ListView_InsertColumn(hwnd(), 0, &column);
}

void UiListView::AddColumn(const std::wstring& title, int width)
{
    int column_count = GetColumnCount();

    LVCOLUMNW column = { 0 };

    column.mask    = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    column.cx      = width;
    column.fmt     = LVCFMT_LEFT;
    column.pszText = const_cast<LPWSTR>(title.c_str());

    ListView_InsertColumn(hwnd(), column_count, &column);
}

void UiListView::DeleteColumn(int column_index)
{
    ListView_DeleteColumn(hwnd(), column_index);
}

void UiListView::DeleteAllColumns()
{
    int count = GetColumnCount();

    while (--count >= 0)
        DeleteColumn(count);
}

void UiListView::SetItemText(int item_index, int column_index, const std::wstring& text)
{
    ListView_SetItemText(hwnd(),
                         item_index,
                         column_index,
                         const_cast<LPWSTR>(text.c_str()));
}

int UiListView::GetItemTextLength(int item_index, int column_index)
{
    LVITEMW item;
    memset(&item, 0, sizeof(item));

    item.iSubItem = column_index;

    return SendMessageW(hwnd(),
                        LVM_GETITEMTEXT,
                        item_index,
                        reinterpret_cast<LPARAM>(&item));
}

std::wstring UiListView::GetItemText(int item_index, int column_index)
{
    int length = GetItemTextLength(item_index, column_index);
    if (length <= 0)
        return std::wstring();

    std::wstring text;
    text.resize(length);

    ListView_GetItemText(hwnd(), item_index, column_index, &text[0], length + 1);

    return text;
}

void UiListView::DeleteAllItems()
{
    ListView_DeleteAllItems(hwnd());
}

void UiListView::SetCheckState(int item_index, bool checked)
{
    ListView_SetCheckState(hwnd(), item_index, checked);
}

bool UiListView::GetCheckState(int item_index)
{
    return !!ListView_GetCheckState(hwnd(), item_index);
}

bool UiListView::ModifyExtendedListViewStyle(DWORD remove, DWORD add)
{
    DWORD style = ListView_GetExtendedListViewStyle(hwnd());

    DWORD new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    ListView_SetExtendedListViewStyle(hwnd(), new_style);

    return true;
}

void UiListView::SetImageList(HIMAGELIST imagelist, int type)
{
    ListView_SetImageList(hwnd(), imagelist, type);
}

int UiListView::GetFirstSelectedItem()
{
    return GetNextSelectedItem(-1);
}

int UiListView::GetNextSelectedItem(int item_index)
{
    return ListView_GetNextItem(hwnd(), item_index, LVNI_SELECTED);
}

int UiListView::GetItemUnderPointer()
{
    LVHITTESTINFO hti;
    memset(&hti, 0, sizeof(hti));

    if (GetCursorPos(&hti.pt))
    {
        if (ScreenToClient(hwnd(), &hti.pt) != FALSE)
        {
            hti.flags = LVHT_ONITEMICON | LVHT_ONITEMLABEL;
            return ListView_HitTest(hwnd(), &hti);
        }
    }

    return -1;
}

UINT UiListView::GetSelectedCount()
{
    return ListView_GetSelectedCount(hwnd());
}

void UiListView::SelectItem(int item_index)
{
    LVITEMW item = { 0 };

    item.mask      = LVIF_STATE;
    item.iItem     = item_index;
    item.state     = LVIS_SELECTED;
    item.stateMask = LVIS_SELECTED;

    ListView_SetItem(hwnd(), &item);
}

HWND UiListView::EditLabel(int item_index)
{
    return ListView_EditLabel(hwnd(), item_index);
}

std::wstring UiListView::GetTextFromEdit()
{
    UiEdit edit(reinterpret_cast<HWND>(SendMessageW(hwnd(),
                                                    LVM_GETEDITCONTROL,
                                                    0,
                                                    0)));
    if (!edit)
        return std::wstring();

    return edit.GetText();
}

} // namespace aspia
