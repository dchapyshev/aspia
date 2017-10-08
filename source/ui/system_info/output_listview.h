//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_listview.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_LISTVIEW_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_LISTVIEW_H

#include "ui/system_info/output.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class OutputListView : protected Output
{
public:
    OutputListView(CListViewCtrl* listview);
    ~OutputListView() = default;

protected:
    // Output implementation.
    void BeginTable(const ColumnList& column_list) final;
    void EndTable() final;
    void BeginItemGroup(const std::string& name) final;
    void EndItemGroup() final;
    void AddItem(const std::string& parameter, const std::string& value) final;
    void AddItem(const std::string& value) final;

private:
    CListViewCtrl* listview_;
    int column_count_ = 0;
    int current_column_ = 0;
    int current_row_ = 0;

    DISALLOW_COPY_AND_ASSIGN(OutputListView);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_LISTVIEW_H
