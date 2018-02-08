//
// PROJECT:         Aspia
// FILE:            ui/auth_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/auth_dialog.h"

#include "base/strings/unicode.h"
#include "crypto/secure_memory.h"
#include "ui/ui_util.h"

namespace aspia {

LRESULT AuthDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    CenterWindow();

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return TRUE;
}

LRESULT AuthDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT AuthDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    username_ = GetWindowString(GetDlgItem(IDC_USERNAME_EDIT));
    password_ = GetWindowString(GetDlgItem(IDC_PASSWORD_EDIT));

    EndDialog(IDOK);
    return 0;
}

LRESULT AuthDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

const std::wstring& AuthDialog::UserName() const
{
    return username_;
}

const std::wstring& AuthDialog::Password() const
{
    return password_.string();
}

} // namespace aspia
