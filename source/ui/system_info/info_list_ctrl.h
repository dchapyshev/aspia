//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/info_list_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H
#define _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H

#include "base/macros.h"
#include "ui/system_info/output.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class InfoListCtrl
    : public CWindowImpl<InfoListCtrl, CListViewCtrl>,
      public Output
{
public:
    InfoListCtrl()
    {
        // Nothing
    }

    ~InfoListCtrl() = default;

    void DeleteAllColumns();
    int GetColumnCount() const;

protected:
    void StartDocument(const std::string& name) final;
    void EndDocument() final;
    void StartTable(const std::string& name, const ColumnList& column_list) final;
    void EndTable() final;
    void StartGroup(const std::string& name, IconId icon_id) final;
    void EndGroup() final;
    void AddParam(Output::IconId icon_id,
                  const std::string& param,
                  const std::string& value,
                  const char* unit) final;
    void StartRow(IconId icon_id) final;
    void EndRow() final;
    void AddValue(const std::string& value, const char* unit) final;

private:
    BEGIN_MSG_MAP(InfoListCtrl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    CImageListManaged imagelist_;

    int column_count_ = 0;
    int current_column_ = 0;
    int indent_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InfoListCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H
