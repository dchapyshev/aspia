//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/viewer_bar.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__VIEWER_BAR_H
#define _ASPIA_GUI__VIEWER_BAR_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class ViewerBar : public CWindowImpl <ViewerBar, CToolBarCtrl>
{
public:
    BEGIN_MSG_MAP(ViewerBar)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)

        NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, OnGetDispInfo)
    END_MSG_MAP()

private:
    LRESULT OnCreate(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled);

    LRESULT OnGetDispInfo(int ctrl_id, LPNMHDR phdr, BOOL &handled);

    void AddIcon(const CSize &size, DWORD resource_id);

private:
    CImageListManaged imagelist_;
};

} // namespace aspia

#endif // _ASPIA_GUI__VIEWER_BAR_H
