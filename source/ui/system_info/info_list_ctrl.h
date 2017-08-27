//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/info_list_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H
#define _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H

#include "base/macros.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class InfoListCtrl : public CWindowImpl<InfoListCtrl, CListViewCtrl>
{
public:
    InfoListCtrl() = default;
    ~InfoListCtrl() = default;

    void DeleteAllColumns();
    int GetColumnCount() const;

private:
    BEGIN_MSG_MAP(InfoListCtrl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(InfoListCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__INFO_LIST_CTRL_H
