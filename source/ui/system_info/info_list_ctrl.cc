//
// PROJECT:         Aspia
// FILE:            ui/system_info/info_list_ctrl.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "ui/system_info/info_list_ctrl.h"

#include <uxtheme.h>
#include "ui/resource.h"

namespace aspia {

LRESULT InfoListCtrl::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    ::SetWindowTheme(*this, L"explorer", nullptr);
    SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

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

void InfoListCtrl::SortColumnItems(int column_index)
{
    if (!sorting_enabled_)
        return;

    SortingContext sorting_context;

    sorting_context.list = this;
    sorting_context.column_index = column_index;

    SortItems(SortingCompare, reinterpret_cast<LPARAM>(&sorting_context));

    sorting_ascending_ = !sorting_ascending_;
}

// static
int CALLBACK InfoListCtrl::SortingCompare(LPARAM lparam1, LPARAM lparam2, LPARAM lparam_sort)
{
    SortingContext* context = reinterpret_cast<SortingContext*>(lparam_sort);

    LVFINDINFOW find_info;
    memset(&find_info, 0, sizeof(find_info));

    find_info.flags = LVFI_PARAM;

    find_info.lParam = lparam1;
    int item_index = context->list->FindItem(&find_info, -1);

    wchar_t item1[256] = { 0 };
    context->list->GetItemText(item_index, context->column_index, item1, ARRAYSIZE(item1));

    find_info.lParam = lparam2;
    item_index = context->list->FindItem(&find_info, -1);

    wchar_t item2[256] = { 0 };
    context->list->GetItemText(item_index, context->column_index, item2, ARRAYSIZE(item2));

    if (context->list->sorting_ascending_)
        return _wcsicmp(item2, item1);
    else
        return _wcsicmp(item1, item2);
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

void InfoListCtrl::StartTable(Category* category)
{
    CIcon icon = AtlLoadIconImage(category->Icon(),
                                  LR_CREATEDIBSECTION,
                                  small_icon_size_.cx,
                                  small_icon_size_.cy);

    if (!imagelist_.GetImageCount())
    {
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_TICK,
                                LR_CREATEDIBSECTION,
                                small_icon_size_.cx,
                                small_icon_size_.cy);
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_DELETE,
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

    if (!sorting_enabled_)
    {
        // By default, sorting must be enabled.
        // If at least one group is added, the sorting will be disabled.
        GetHeader().ModifyStyle(0, HDS_BUTTONS);
        sorting_enabled_ = true;
    }
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
    item.lParam  = item_count_;

    InsertItem(&item);

    ++indent_;
    ++item_count_;

    // Categories that contain groups can not be sorted by a column.
    if (sorting_enabled_)
    {
        // Disable the possibility of sorting in the control.
        GetHeader().ModifyStyle(HDS_BUTTONS, 0);
        sorting_enabled_ = false;
    }
}

void InfoListCtrl::EndGroup()
{
    --indent_;
}

std::wstring InfoListCtrl::ValueToString(const Value& value)
{
    switch (value.type())
    {
        case Value::Type::BOOL:
            return value.ToBool() ? L"Yes" : L"No";

        case Value::Type::STRING:
            return UNICODEfromUTF8(value.ToString().data());

        case Value::Type::INT32:
            return std::to_wstring(value.ToInt32());

        case Value::Type::UINT32:
            return std::to_wstring(value.ToUint32());

        case Value::Type::INT64:
            return std::to_wstring(value.ToInt64());

        case Value::Type::UINT64:
            return std::to_wstring(value.ToUint64());

        case Value::Type::DOUBLE:
            return std::to_wstring(value.ToDouble());

        case Value::Type::MEMORY_SIZE:
            return StringPrintf(L"%.2f", value.ToMemorySize());

        default:
        {
            DLOG(LS_FATAL) << "Unhandled value type: " << static_cast<int>(value.type());
            return std::wstring();
        }
    }
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
    item.mask     = LVIF_IMAGE | LVIF_INDENT | LVIF_TEXT | LVIF_PARAM;
    item.iImage   = icon_index;
    item.iIndent  = indent_;
    item.pszText  = const_cast<LPWSTR>(param_name.c_str());
    item.iItem    = item_count_;
    item.lParam   = item_count_;
    item.iSubItem = 0;

    const int item_index = InsertItem(&item);

    std::wstring text = ValueToString(value);

    if (value.HasUnit())
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(value.Unit().data()));
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
    std::wstring text = ValueToString(value);

    if (value.HasUnit())
    {
        text.append(L" ");
        text.append(UNICODEfromUTF8(value.Unit().data()));
    }

    if (current_column_ == 0)
    {
        LVITEMW item = { 0 };

        item.mask     = LVIF_TEXT | LVIF_STATE | LVIF_IMAGE | LVIF_INDENT | LVIF_PARAM;
        item.iImage   = ICON_INDEX_CATEGORY;
        item.pszText  = const_cast<LPWSTR>(text.c_str());
        item.iItem    = item_count_;
        item.lParam   = item_count_;
        item.iSubItem = current_column_;
        item.iIndent  = indent_;

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
