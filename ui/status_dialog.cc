//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/status_dialog.h"
#include "ui/resource.h"
#include "ui/base/edit.h"
#include "ui/base/module.h"
#include "base/logging.h"
#include "base/unicode.h"
#include "base/string_util.h"

#include <windowsx.h>

namespace aspia {

INT_PTR StatusDialog::DoModal(HWND parent, Delegate* delegate)
{
    delegate_ = delegate;
    return DoModal(parent);
}

INT_PTR StatusDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_STATUS);
}

void StatusDialog::SetDestonation(const std::wstring& address, uint16_t port)
{
    std::wstring format = module().string(IDS_CONNECTION);

    std::wstring message =
        StringPrintfW(format.c_str(), address.c_str(), port);

    SetWindowTextW(hwnd(), message.c_str());
}

// static
std::wstring StatusDialog::StatusToString(proto::Status status)
{
    UINT resource_id = status + IDS_STATUS_MIN;

    if (!Status_IsValid(status))
        resource_id = IDS_STATUS_UNKNOWN;

    return Module().string(resource_id);
}

void StatusDialog::SetStatus(proto::Status status)
{
    AddMessage(StatusToString(status));
}

void StatusDialog::OnInitDialog()
{
    SetForegroundWindowEx();
    SetIcon(IDI_MAIN);
    SetFocus(GetDlgItem(IDCANCEL));

    delegate_->OnStatusDialogOpen();
}

void StatusDialog::AddMessage(const std::wstring& message)
{
    Edit status_edit(GetDlgItem(IDC_STATUS_EDIT));

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

INT_PTR StatusDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
