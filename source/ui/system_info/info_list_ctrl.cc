//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/info_list_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
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

void InfoListCtrl::StartDocument(const std::string& name)
{
    // Nothing
}

void InfoListCtrl::EndDocument()
{
    // Nothing
}

void InfoListCtrl::StartTable(const std::string& name)
{
    UNREF(name);
    imagelist_.RemoveAll();
    indent_ = 0;
}

void InfoListCtrl::EndTable()
{
    // Nothing
}

void InfoListCtrl::StartTableHeader()
{
    current_column_ = 0;
    column_count_ = 0;
}

void InfoListCtrl::EndTableHeader()
{
    column_count_ = current_column_ + 1;
    current_column_ = 0;
}

void InfoListCtrl::AddHeaderItem(const std::string& name, int width)
{
    const int column_index = AddColumn(UNICODEfromUTF8(name).c_str(), current_column_);
    SetColumnWidth(column_index, width);
    ++current_column_;
}

void InfoListCtrl::StartGroup(const std::string& name, Category::IconId icon_id)
{
    LVITEMW item = { 0 };

    const int icon_index = imagelist_.AddIcon(
        AtlLoadIconImage(icon_id,
                         LR_CREATEDIBSECTION,
                         GetSystemMetrics(SM_CXSMICON),
                         GetSystemMetrics(SM_CYSMICON)));

    std::wstring text(UNICODEfromUTF8(name));

    item.mask    = LVIF_IMAGE | LVIF_INDENT | LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    item.iImage  = icon_index;
    item.iIndent = indent_;
    item.pszText = const_cast<LPWSTR>(text.c_str());
    item.iItem   = GetItemCount();
    item.lParam  = 0;

    InsertItem(&item);

    ++indent_;
}

void InfoListCtrl::EndGroup()
{
    --indent_;
}

void InfoListCtrl::AddParam(Category::IconId icon_id,
                            const std::string& param,
                            const std::string& value,
                            const char* unit)
{
    LVITEMW item = { 0 };

    const int icon_index = imagelist_.AddIcon(
        AtlLoadIconImage(icon_id,
                         LR_CREATEDIBSECTION,
                         GetSystemMetrics(SM_CXSMICON),
                         GetSystemMetrics(SM_CYSMICON)));

    std::wstring param_name(UNICODEfromUTF8(param));

    item.mask     = LVIF_IMAGE | LVIF_INDENT | LVIF_TEXT;
    item.iImage   = icon_index;
    item.iIndent  = indent_;
    item.pszText  = const_cast<LPWSTR>(param_name.c_str());
    item.iItem    = GetItemCount();
    item.iSubItem = 0;

    const int item_index = InsertItem(&item);

    std::wstring text(UNICODEfromUTF8(value));

    if (unit)
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(unit));
    }

    AddItem(item_index, 1, text.c_str());
}

void InfoListCtrl::StartRow(Category::IconId icon_id)
{
    current_column_ = 0;

    imagelist_.AddIcon(AtlLoadIconImage(icon_id,
                                        LR_CREATEDIBSECTION,
                                        GetSystemMetrics(SM_CXSMICON),
                                        GetSystemMetrics(SM_CYSMICON)));
}

void InfoListCtrl::EndRow()
{
    DCHECK(current_column_ == column_count_ - 1);
}

void InfoListCtrl::AddValue(const std::string& value, const char* unit)
{
    std::wstring text(UNICODEfromUTF8(value));

    if (unit)
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(unit));
    }

    if (current_column_ == 0)
    {
        LVITEMW item = { 0 };

        item.mask     = LVIF_TEXT | LVIF_STATE | LVIF_IMAGE;
        item.iImage   = imagelist_.GetImageCount() - 1;
        item.pszText  = const_cast<LPWSTR>(text.c_str());
        item.iItem    = GetItemCount();
        item.iSubItem = current_column_;

        InsertItem(&item);
    }
    else
    {
        SetItemText(GetItemCount() - 1, current_column_, text.c_str());
    }

    ++current_column_;
}

} // namespace aspia
