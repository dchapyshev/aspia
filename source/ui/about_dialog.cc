//
// PROJECT:         Aspia
// FILE:            ui/about_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/about_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "network/url.h"

namespace aspia {

LRESULT AboutDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    icon_ = AtlLoadIconImage(IDI_MAIN, LR_CREATEDIBSECTION, 48, 48);

    CStatic(GetDlgItem(IDC_ABOUT_ICON)).SetIcon(icon_);

    CString about_text;
    about_text.LoadStringW(IDS_ABOUT_STRING);

    CEdit(GetDlgItem(IDC_ABOUT_EDIT)).SetWindowTextW(about_text);
    GetDlgItem(IDC_DONATE_BUTTON).SetFocus();

    return FALSE;
}

LRESULT AboutDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

LRESULT AboutDialog::OnDrawItem(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);

    int saved_dc = SaveDC(dis->hDC);

    if (saved_dc)
    {
        // Transparent background.
        SetBkMode(dis->hDC, TRANSPARENT);

        HBRUSH background_brush = GetSysColorBrush(COLOR_WINDOW);
        FillRect(dis->hDC, &dis->rcItem, background_brush);
        DrawEdge(dis->hDC, &dis->rcItem, EDGE_BUMP, BF_RECT | BF_FLAT | BF_MONO);

        RestoreDC(dis->hDC, saved_dc);
    }

    return 0;
}

LRESULT AboutDialog::OnCloseButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

LRESULT AboutDialog::OnDonateButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CString url;
    url.LoadStringW(IDS_DONATE_LINK);
    URL::FromCString(url).Open();
    return 0;
}

LRESULT AboutDialog::OnSiteButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CString url;
    url.LoadStringW(IDS_SITE_LINK);
    URL::FromCString(url).Open();
    return 0;
}

} // namespace aspia
