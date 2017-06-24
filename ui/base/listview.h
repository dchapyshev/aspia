//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/listview.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__LISTVIEW_H
#define _ASPIA_UI__BASE__LISTVIEW_H

#include "ui/base/window.h"

#include <commctrl.h>
#include <string>

namespace aspia {

class UiListView : public UiWindow
{
public:
    UiListView() = default;
    UiListView(HWND hwnd);

    bool Create(HWND parent, DWORD ex_style, DWORD style, HINSTANCE instance);

    int GetColumnCount();
    int GetItemCount();

    void AddOnlyOneColumn(const std::wstring& title = std::wstring());

    void AddColumn(const std::wstring& title, int width);
    void DeleteColumn(int column_index);
    void DeleteAllColumns();

    template <typename T>
    int AddItem(const std::wstring& text, T item_data, int image_index = -1)
    {
        LVITEMW item = { 0 };

        item.mask    = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        item.pszText = const_cast<LPWSTR>(text.c_str());
        item.iItem   = GetItemCount();
        item.lParam  = static_cast<LPARAM>(item_data);
        item.iImage  = image_index;

        return ListView_InsertItem(hwnd(), &item);
    }

    void SetItemText(int item_index, int column_index, const std::wstring& text);

    template <typename T>
    T GetItemData(int item_index)
    {
        LVITEMW item = { 0 };

        item.mask  = LVIF_PARAM;
        item.iItem = item_index;

        if (!ListView_GetItem(hwnd(), &item))
            return 0;

        return static_cast<T>(item.lParam);
    }

    int GetItemTextLength(int item_index, int column_index);
    std::wstring GetItemText(int item_index, int column_index);

    void DeleteAllItems();
    void SetCheckState(int item_index, bool checked);
    bool GetCheckState(int item_index);
    bool ModifyExtendedListViewStyle(DWORD remove, DWORD add);
    void SetImageList(HIMAGELIST imagelist, int type);
    int GetFirstSelectedItem();
    int GetItemUnderPointer();
    UINT GetSelectedCount();

    HWND EditLabel(int item_index);
    std::wstring GetTextFromEdit();
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__LISTVIEW_H
