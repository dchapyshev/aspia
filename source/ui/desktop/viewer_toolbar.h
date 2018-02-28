//
// PROJECT:         Aspia
// FILE:            ui/desktop/viewer_toolbar.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__DESKTOP__VIEWER_TOOLBAR_H
#define _ASPIA_UI__DESKTOP__VIEWER_TOOLBAR_H

#include "base/macros.h"
#include "proto/auth_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class ViewerToolBar : public CWindowImpl<ViewerToolBar, CToolBarCtrl>
{
public:
    ViewerToolBar() = default;
    ~ViewerToolBar() = default;

    bool CreateViewerToolBar(HWND parent, proto::auth::SessionType session_type);

private:
    BEGIN_MSG_MAP(ViewerToolBar)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnGetDispInfo)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled);

    void AddIcon(UINT icon_id, const CSize& icon_size);

    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(ViewerToolBar);
};

} // namespace aspia

#endif // _ASPIA_UI__DESKTOP__VIEWER_TOOLBAR_H
