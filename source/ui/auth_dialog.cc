//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/auth_dialog.h"
#include "base/strings/unicode.h"

#include <atlctrls.h>

namespace aspia {

LRESULT UiAuthDialog::OnInitDialog(UINT message,
                                   WPARAM wparam,
                                   LPARAM lparam,
                                   BOOL& handled)
{
    CenterWindow();

    DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return TRUE;
}

LRESULT UiAuthDialog::OnClose(UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL& handled)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT UiAuthDialog::OnOkButton(WORD notify_code,
                                 WORD control_id,
                                 HWND control,
                                 BOOL& handled)
{
    // TODO: Clear memory.

    WCHAR buffer[128];

    GetDlgItemTextW(IDC_USERNAME_EDIT, buffer, _countof(buffer));
    UNICODEtoUTF8(buffer, username_);

    GetDlgItemTextW(IDC_PASSWORD_EDIT, buffer, _countof(buffer));
    UNICODEtoUTF8(buffer, password_);

    EndDialog(IDOK);
    return 0;
}

LRESULT UiAuthDialog::OnCancelButton(WORD notify_code,
                                     WORD control_id,
                                     HWND control,
                                     BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

const std::string& UiAuthDialog::UserName() const
{
    return username_;
}

const std::string& UiAuthDialog::Password() const
{
    return password_;
}

} // namespace aspia
