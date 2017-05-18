//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/status_dialog.h"
#include "ui/resource.h"
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

void StatusDialog::SetStatus(ClientStatus status)
{
    UINT resource_id = 0;

    switch (status)
    {
        case ClientStatus::INVALID_HOSTNAME:
            resource_id = IDS_STATUS_INVALID_HOSTNAME;
            break;

        case ClientStatus::INVALID_PORT:
            resource_id = IDS_STATUS_INVALID_PORT;
            break;

        case ClientStatus::DISCONNECTED:
            resource_id = IDS_STATUS_DISCONNECTED;
            break;

        case ClientStatus::CONNECTING:
            resource_id = IDS_STATUS_CONNECTING;
            break;

        case ClientStatus::CONNECTED:
            resource_id = IDS_STATUS_CONNECTED;
            break;

        case ClientStatus::CONNECT_TIMEOUT:
            resource_id = IDS_STATUS_CONNECT_TIMEOUT;
            break;

        case ClientStatus::CONNECT_ERROR:
            resource_id = IDS_STATUS_CONNECT_ERROR;
            break;

        case ClientStatus::INVALID_USERNAME_OR_PASSWORD:
            resource_id = IDS_STATUS_BAD_USERNAME_OR_PASSWORD;
            break;

        case ClientStatus::NOT_SUPPORTED_AUTH_METHOD:
            resource_id = IDS_STATUS_NOT_SUPPORTED_METHOD;
            break;

        case ClientStatus::SESSION_TYPE_NOT_ALLOWED:
            resource_id = IDS_STATUS_SESSION_TYPE_NOT_ALLOWED;
            break;

        default:
            resource_id = IDS_STATUS_UNKNOWN;
            break;
    }

    AddMessage(resource_id);
}

void StatusDialog::OnInitDialog()
{
    SetIcon(IDI_MAIN);
    SetFocus(GetDlgItem(IDCANCEL));

    delegate_->OnStatusDialogOpen();
}

void StatusDialog::OnClose()
{
    EndDialog(0);
}

void StatusDialog::OnCancelButton()
{
    EndDialog(0);
}

static void AppendText(HWND edit, const WCHAR* text)
{
    int length = GetWindowTextLengthW(edit);

    Edit_SetSel(edit, length, length);
    Edit_ScrollCaret(edit);
    Edit_ReplaceSel(edit, text);
}

void StatusDialog::AddMessage(UINT resource_id)
{
    HWND status_edit = GetDlgItem(IDC_STATUS_EDIT);

    WCHAR buffer[128];

    if (GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
    {
        AppendText(status_edit, buffer);
        AppendText(status_edit, L" ");
    }

    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
    {
        AppendText(status_edit, buffer);
        AppendText(status_edit, L": ");
    }

    AppendText(status_edit, module().string(resource_id).c_str());
    AppendText(status_edit, L"\r\n");
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
                    OnCancelButton();
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
