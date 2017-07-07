//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_toolbar.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TOOLBAR_H
#define _ASPIA_UI__FILE_TOOLBAR_H

#include "base/macros.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiFileToolBar : public CWindowImpl<UiFileToolBar, CToolBarCtrl>
{
public:
    enum class Type { LOCAL, REMOTE };

    UiFileToolBar(Type type);
    virtual ~UiFileToolBar() = default;

    bool CreateFileToolBar(HWND parent);

private:
    BEGIN_MSG_MAP(UiFileToolBar)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnGetDispInfo)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled);

    void AddIcon(UINT icon_id, const CSize& icon_size);
    void SetButtonText(int command_id, UINT resource_id);

    const Type type_;
    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(UiFileToolBar);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TOOLBAR_H
