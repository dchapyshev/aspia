//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/about_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/about_dialog.h"
#include "ui/resource.h"
#include "base/logging.h"

#include <windowsx.h>
#include <shellapi.h>

namespace aspia {

INT_PTR AboutDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_ABOUT);
}

void AboutDialog::OnInitDialog()
{
    icon_ = module().icon(IDI_MAIN, 48, 48, LR_CREATEDIBSECTION);
    Static_SetIcon(GetDlgItem(IDC_ABOUT_ICON), icon_);

    SetWindowTextW(GetDlgItem(IDC_ABOUT_EDIT),
                   module().string(IDS_ABOUT_STRING).c_str());

    SetFocus(GetDlgItem(IDC_DONATE_BUTTON));
}

void AboutDialog::OnClose()
{
    EndDialog(0);
}

void AboutDialog::OnCloseButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
}

void AboutDialog::OnDonateButton()
{
    ShellExecuteW(nullptr,
                  L"open",
                  module().string(IDS_DONATE_LINK).c_str(),
                  nullptr,
                  nullptr,
                  SW_SHOWNORMAL);
}

void AboutDialog::OnSiteButton()
{
    ShellExecuteW(nullptr,
                  L"open",
                  module().string(IDS_SITE_LINK).c_str(),
                  nullptr,
                  nullptr,
                  SW_SHOWNORMAL);
}

INT_PTR AboutDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDOK:
                    OnCloseButton();
                    break;

                case IDC_DONATE_BUTTON:
                    OnDonateButton();
                    break;

                case IDC_SITE_BUTTON:
                    OnSiteButton();
                    break;
            }
        }
        break;

        case WM_CLOSE:
            OnClose();
            break;
    }

    return 0;
}

} // namespace aspia
