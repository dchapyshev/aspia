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
#include <atlmisc.h>

namespace aspia {

class InfoListCtrl
    : public CWindowImpl<InfoListCtrl, CListViewCtrl>,
      public Output
{
public:
    InfoListCtrl() = default;
    ~InfoListCtrl() = default;

    void DeleteAllColumns();
    int GetColumnCount() const;

protected:
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(std::string_view name) final;
    void EndTableGroup() final;
    void StartTable(std::string_view name) final;
    void EndTable() final;
    void StartTableHeader() final;
    void EndTableHeader() final;
    void AddHeaderItem(std::string_view name, int width) final;
    void StartGroup(std::string_view name, Category::IconId icon_id) final;
    void EndGroup() final;
    void AddParam(Category::IconId icon_id,
                  std::string_view param,
                  std::string_view value,
                  std::string_view unit) final;
    void StartRow(Category::IconId icon_id) final;
    void EndRow() final;
    void AddValue(std::string_view value, std::string_view unit) final;

private:
    BEGIN_MSG_MAP(InfoListCtrl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    CSize small_icon_size_;
    CImageListManaged imagelist_;

    int item_count_ = 0;
    int column_count_ = 0;
    int current_column_ = 0;
    int indent_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InfoListCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H
