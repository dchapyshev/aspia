//
// PROJECT:         Aspia
// FILE:            ui/auth_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/auth_dialog.h"
#include "base/strings/unicode.h"
#include "crypto/secure_memory.h"

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
    SecureArray<WCHAR, 128> buffer;

    GetDlgItemTextW(IDC_USERNAME_EDIT, buffer.get(), static_cast<int>(buffer.count()));
    UNICODEtoUTF8(buffer.get(), username_);

    GetDlgItemTextW(IDC_PASSWORD_EDIT, buffer.get(), static_cast<int>(buffer.count()));
    UNICODEtoUTF8(buffer.get(), password_.mutable_string());

    EndDialog(IDOK);
    return 0;
}

LRESULT AuthDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

const std::string& AuthDialog::UserName() const
{
    return username_;
}

const std::string& AuthDialog::Password() const
{
    return password_.string();
}

} // namespace aspia
