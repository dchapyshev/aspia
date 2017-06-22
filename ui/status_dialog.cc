//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/status_dialog.h"
#include "ui/status_code.h"
#include "ui/resource.h"
#include "ui/base/edit.h"
#include "ui/base/module.h"
#include "base/logging.h"
#include "base/unicode.h"
#include "base/string_util.h"

#include <windowsx.h>

namespace aspia {

INT_PTR UiStatusDialog::DoModal(HWND parent, Delegate* delegate)
{
    delegate_ = delegate;
    return DoModal(parent);
}

INT_PTR UiStatusDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_STATUS);
}

void UiStatusDialog::SetDestonation(const std::wstring& address, uint16_t port)
{
    std::wstring format = module().string(IDS_CONNECTION);

    std::wstring message =
        StringPrintfW(format.c_str(), address.c_str(), port);

    SetWindowTextW(hwnd(), message.c_str());
}

void UiStatusDialog::SetStatus(proto::Status status)
{
    AddMessage(StatusCodeToString(module(), status));
}

void UiStatusDialog::OnInitDialog()
{
    SetForegroundWindowEx();
    SetIcon(IDI_MAIN);
    SetFocus(GetDlgItem(IDCANCEL));

    delegate_->OnStatusDialogOpen();
}

void UiStatusDialog::AddMessage(const std::wstring& message)
{
    UiEdit status_edit(GetDlgItem(IDC_STATUS_EDIT));

    WCHAR buffer[128];

    if (GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
    {
        status_edit.AppendText(buffer);
        status_edit.AppendText(L" ");
    }

    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
    {
        status_edit.AppendText(buffer);
        status_edit.AppendText(L": ");
    }

    status_edit.AppendText(message);
    status_edit.AppendText(L"\r\n");
}

INT_PTR UiStatusDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
                case IDCANCEL:
                    EndDialog();
                    break;
            }
        }
        break;

        case WM_CLOSE:
            EndDialog();
            break;
    }

    return 0;
}

} // namespace aspia
