//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/info_list_ctrl.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "ui/system_info/info_list_ctrl.h"

#include <uxtheme.h>
#include "ui/resource.h"

namespace aspia {

LRESULT InfoListCtrl::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    SetExtendedListViewStyle(ex_style);

    small_icon_size_.SetSize(GetSystemMetrics(SM_CXSMICON),
                             GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size_.cx,
                          small_icon_size_.cy,
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

void InfoListCtrl::StartDocument()
{
    // Nothing
}

void InfoListCtrl::EndDocument()
{
    EnableWindow(item_count_ > 0 ? TRUE : FALSE);
}

void InfoListCtrl::StartTableGroup(std::string_view /* name */)
{
    // Nothing
}

void InfoListCtrl::EndTableGroup()
{
    // Nothing
}

void InfoListCtrl::StartTable(Category* category, Table::Type /* table_type */)
{
    CIcon icon = AtlLoadIconImage(category->Icon(),
                                  LR_CREATEDIBSECTION,
                                  small_icon_size_.cx,
                                  small_icon_size_.cy);

    if (!imagelist_.GetImageCount())
    {
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_CHECKED,
                                LR_CREATEDIBSECTION,
                                small_icon_size_.cx,
                                small_icon_size_.cy);
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_UNCHECKED,
                                LR_CREATEDIBSECTION,
                                small_icon_size_.cx,
                                small_icon_size_.cy);
        imagelist_.AddIcon(icon);
    }
    else
    {
        imagelist_.ReplaceIcon(ICON_INDEX_CATEGORY, icon);
    }

    item_count_ = 0;
    indent_ = 0;
}

void InfoListCtrl::EndTable()
{
    // Nothing
}

void InfoListCtrl::AddColumns(const ColumnList& column_list)
{
    current_column_ = 0;
    column_count_ = 0;

    for (const auto& column : column_list)
    {
        const int column_index = AddColumn(UNICODEfromUTF8(column.first).c_str(), current_column_);
        SetColumnWidth(column_index, column.second);
        ++current_column_;
    }

    column_count_ = current_column_ + 1;
    current_column_ = 0;
}

void InfoListCtrl::StartGroup(std::string_view name)
{
    std::wstring text(UNICODEfromUTF8(name.data()));

    LVITEMW item = { 0 };
    item.mask    = LVIF_IMAGE | LVIF_INDENT | LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    item.iImage  = ICON_INDEX_CATEGORY;
    item.iIndent = indent_;
    item.pszText = const_cast<LPWSTR>(text.c_str());
    item.iItem   = item_count_;
    item.lParam  = 0;

    InsertItem(&item);

    ++indent_;
    ++item_count_;
}

void InfoListCtrl::EndGroup()
{
    --indent_;
}

void InfoListCtrl::AddParam(std::string_view param, const Value& value)
{
    int icon_index;

    switch (value.type())
    {
        case Value::Type::BOOL:
            icon_index = value.ToBool() ? ICON_INDEX_CHECKED : ICON_INDEX_UNCHECKED;
            break;

        default:
            icon_index = ICON_INDEX_CATEGORY;
            break;
    }

    std::wstring param_name(UNICODEfromUTF8(param.data()));

    LVITEMW item = { 0 };
    item.mask     = LVIF_IMAGE | LVIF_INDENT | LVIF_TEXT;
    item.iImage   = icon_index;
    item.iIndent  = indent_;
    item.pszText  = const_cast<LPWSTR>(param_name.c_str());
    item.iItem    = item_count_;
    item.iSubItem = 0;

    const int item_index = InsertItem(&item);

    std::wstring text(UNICODEfromUTF8(value.ToString()));

    if (value.HasUnit())
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(value.Unit()));
    }

    AddItem(item_index, 1, text.c_str());
    ++item_count_;
}

void InfoListCtrl::StartRow()
{
    current_column_ = 0;
}

void InfoListCtrl::EndRow()
{
    DCHECK(current_column_ == column_count_ - 1);
}

void InfoListCtrl::AddValue(const Value& value)
{
    std::wstring text(UNICODEfromUTF8(value.ToString()));

    if (value.HasUnit())
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(value.Unit()));
    }

    if (current_column_ == 0)
    {
        LVITEMW item = { 0 };

        item.mask     = LVIF_TEXT | LVIF_STATE | LVIF_IMAGE;
        item.iImage   = ICON_INDEX_CATEGORY;
        item.pszText  = const_cast<LPWSTR>(text.c_str());
        item.iItem    = item_count_;
        item.iSubItem = current_column_;

        InsertItem(&item);
        ++item_count_;
    }
    else
    {
        SetItemText(item_count_ - 1, current_column_, text.c_str());
    }

    ++current_column_;
}

} // namespace aspia
