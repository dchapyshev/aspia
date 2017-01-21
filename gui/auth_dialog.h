//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/auth_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__AUTH_DIALOG_H
#define _ASPIA_GUI__AUTH_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlcrack.h>

#include "gui/resource.h"

namespace aspia {

class CAuthDialog : public CDialogImpl<CAuthDialog>
{
public:
    enum { IDD = IDD_AUTH };

    BEGIN_MSG_MAP(CAuthDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
};

} // namespace aspia

#endif // _ASPIA_GUI__AUTH_DIALOG_H
