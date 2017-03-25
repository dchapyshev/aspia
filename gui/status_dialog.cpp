//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/status_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/status_dialog.h"
#include "base/unicode.h"

namespace aspia {

extern CIcon _small_icon;
extern CIcon _big_icon;

StatusDialog::StatusDialog(const ClientConfig& config) : config_(config)
{
    // Nothing
}

BOOL StatusDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    SetIcon(_small_icon, FALSE);
    SetIcon(_big_icon, TRUE);

    CenterWindow();

    CString message;
    message.Format(IDS_STATUS_CONNECTING_FORMAT,
                   UNICODEfromUTF8(config_.RemoteAddress()).c_str(),
                   config_.RemotePort());
    AddMessage(message);

    Connect(config_.RemoteAddress(), config_.RemotePort());

    GetDlgItem(IDCANCEL).SetFocus();

    return FALSE;
}

void StatusDialog::OnClose()
{
    viewer_window_.reset();
    EndDialog(0);
}

LRESULT StatusDialog::OnAppMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT StatusDialog::OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void StatusDialog::OnConnectionSuccess(std::unique_ptr<Socket> sock)
{
    AddMessage(IDS_STATUS_CONNECTED);
    ShowWindow(SW_HIDE);

    std::unique_ptr<ViewerWindow> viewer_window(new ViewerWindow(this, std::move(sock), &config_));

    viewer_window_.reset(new DialogThread<ViewerWindow>(std::move(viewer_window)));
}

void StatusDialog::OnConnectionError()
{
    AddMessage(IDS_STATUS_NOT_CONNECTED);
}

void StatusDialog::AddMessage(const WCHAR* message)
{
    CEdit status = GetDlgItem(IDC_STATUS_EDIT);

    WCHAR buffer[128];

    if (GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, ARRAYSIZE(buffer)))
    {
        status.AppendText(buffer);
        status.AppendText(L" ");
    }

    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, ARRAYSIZE(buffer)))
    {
        status.AppendText(buffer);
        status.AppendText(L": ");
    }

    status.AppendText(message);
    status.AppendText(L"\r\n");
}

void StatusDialog::AddMessage(UINT message_id)
{
    CString message;
    message.LoadStringW(message_id);
    AddMessage(message);
}

} // namespace aspia
