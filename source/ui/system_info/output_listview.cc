//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_listview.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "ui/system_info/output_listview.h"

namespace aspia {

OutputListView::OutputListView(CListViewCtrl* listview)
    : listview_(listview)
{
    // Nothing
}

void OutputListView::BeginTable(const ColumnList& column_list)
{
    int current_column = 0;

    for (auto& column : column_list)
    {
        listview_->AddColumn(column.Name(), current_column);
        ++current_column;
    }

    column_count_ = column_list.size();
}

void OutputListView::EndTable()
{
    current_row_ = 0;
    current_column_ = 0;
}

void OutputListView::BeginItemGroup(const std::string& name)
{
    listview_->AddItem(current_row_, 0, UNICODEfromUTF8(name).c_str());
    ++current_row_;
}

void OutputListView::EndItemGroup()
{
    current_row_ = 0;
    current_column_ = 0;
}

void OutputListView::AddItem(const std::string& parameter, const std::string& value)
{
    listview_->AddItem(current_row_, 0, UNICODEfromUTF8(parameter).c_str());
    listview_->AddItem(current_row_, 1, UNICODEfromUTF8(value).c_str());
    current_column_ = 0;
    ++current_row_;
}

void OutputListView::AddItem(const std::string& value)
{
    listview_->AddItem(current_row_, current_column_, UNICODEfromUTF8(value).c_str());

    ++current_column_;

    if (current_column_ >= column_count_)
    {
        current_column_ = 0;
        ++current_row_;
    }
}

} // namespace aspia
