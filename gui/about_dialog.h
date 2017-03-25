//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/about_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__ABOUT_DIALOG_H
#define _ASPIA_GUI__ABOUT_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <atlcrack.h>
#include <atlctrls.h>

#include "gui/resource.h"

namespace aspia {

class AboutDialog : public CDialogImpl<AboutDialog>
{
public:
    enum { IDD = IDD_ABOUT };

private:
    BEGIN_MSG_MAP(AboutDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)

        COMMAND_ID_HANDLER(IDOK, OnCloseButton)
        COMMAND_ID_HANDLER(IDC_DONATE_BUTTON, OnDonateButton)
        COMMAND_ID_HANDLER(IDC_SITE_BUTTON, OnSiteButton)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();

    LRESULT OnCloseButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnDonateButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnSiteButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);

private:
    CIcon icon_;
};

} // namespace aspia

#endif // _ASPIA_GUI__ABOUT_DIALOG_H
