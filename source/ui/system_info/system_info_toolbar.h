//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/system_info_toolbar.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__SYSTEM_INFO_TOOLBAR_H
#define _ASPIA_UI__SYSTEM_INFO__SYSTEM_INFO_TOOLBAR_H

#include "base/macros.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class SystemInfoToolbar : public CWindowImpl<SystemInfoToolbar, CToolBarCtrl>
{
public:
    SystemInfoToolbar() = default;
    ~SystemInfoToolbar() = default;

private:
    BEGIN_MSG_MAP(SystemInfoToolbar)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnGetDispInfo)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled);

    void AddIcon(UINT icon_id, const CSize& icon_size);
    void SetButtonText(int command_id, UINT resource_id);

    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(SystemInfoToolbar);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__SYSTEM_INFO_TOOLBAR_H
