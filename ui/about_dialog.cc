//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/about_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/about_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>
#include <shellapi.h>

namespace aspia {

LRESULT UiAboutDialog::OnInitDialog(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    icon_ = AtlLoadIconImage(IDI_MAIN, LR_CREATEDIBSECTION, 48, 48);

    CStatic(GetDlgItem(IDC_ABOUT_ICON)).SetIcon(icon_);

    CString about_text;
    about_text.LoadStringW(IDS_ABOUT_STRING);

    CEdit(GetDlgItem(IDC_ABOUT_EDIT)).SetWindowTextW(about_text);

    GetDlgItem(IDC_DONATE_BUTTON).SetFocus();

    return 0;
}

LRESULT UiAboutDialog::OnClose(UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiAboutDialog::OnCloseButton(WORD notify_code,
                                     WORD control_id,
                                     HWND control,
                                     BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiAboutDialog::OnDonateButton(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    CString url;
    url.LoadStringW(IDS_DONATE_LINK);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT UiAboutDialog::OnSiteButton(WORD notify_code,
                                    WORD control_id,
                                    HWND control,
                                    BOOL& handled)
{
    CString url;
    url.LoadStringW(IDS_SITE_LINK);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

} // namespace aspia
