//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/about_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ABOUT_DIALOG_H
#define _ASPIA_UI__ABOUT_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>

#include "base/macros.h"
#include "ui/resource.h"

namespace aspia {

class UiAboutDialog :
    public CDialogImpl<UiAboutDialog>,
    public CDialogResize<UiAboutDialog>
{
public:
    enum { IDD = IDD_ABOUT };

    UiAboutDialog() = default;

private:
    BEGIN_MSG_MAP(UiAboutDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)

        COMMAND_ID_HANDLER(IDOK, OnCloseButton)
        COMMAND_ID_HANDLER(IDC_DONATE_BUTTON, OnDonateButton)
        COMMAND_ID_HANDLER(IDC_SITE_BUTTON, OnSiteButton)

        CHAIN_MSG_MAP(CDialogResize<UiAboutDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(UiAboutDialog)
        DLGRESIZE_CONTROL(IDC_ABOUT_EDIT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDC_BUTTON_GROUP, DLSZ_MOVE_Y | DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_DONATE_BUTTON, DLSZ_MOVE_Y | DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SITE_BUTTON, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCloseButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDonateButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSiteButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    CIcon icon_;

    DISALLOW_COPY_AND_ASSIGN(UiAboutDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__ABOUT_DIALOG_H
