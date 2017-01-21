//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/about_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/about_dialog.h"

namespace aspia {

BOOL AboutDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    CenterWindow();

    icon_ = AtlLoadIconImage(IDI_MAINICON, LR_CREATEDIBSECTION, 48, 48);

    CStatic(GetDlgItem(IDC_ABOUT_ICON)).SetIcon(icon_);

    CString about;
    about.LoadStringW(IDS_ABOUT_STRING);

    CEdit edit(GetDlgItem(IDC_ABOUT_EDIT));

    edit.AppendText(about);
    edit.SetSelNone();

    GetDlgItem(IDC_DONATE_BUTTON).SetFocus();

    return FALSE;
}

void AboutDialog::OnClose()
{
    EndDialog(0);
}

LRESULT AboutDialog::OnCloseButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT AboutDialog::OnDonateButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CString link;
    link.LoadStringW(IDS_DONATE_LINK);

    ShellExecuteW(nullptr, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);

    return 0;
}

LRESULT AboutDialog::OnSiteButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CString link;
    link.LoadStringW(IDS_SITE_LINK);

    ShellExecuteW(nullptr, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);

    return 0;
}

} // namespace aspia
