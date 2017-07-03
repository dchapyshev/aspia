//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/status_dialog.h"
#include "ui/status_code.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

namespace aspia {

UiStatusDialog::UiStatusDialog(Delegate* delegate) :
    delegate_(delegate)
{
    // Nothing
}

void UiStatusDialog::SetDestonation(const std::wstring& address, uint16_t port)
{
    CString message;
    message.Format(IDS_CONNECTION, address.c_str(), port);
    SetWindowTextW(message);
}

void UiStatusDialog::SetStatus(proto::Status status)
{
    AddMessage(StatusCodeToString(status));
}

LRESULT UiStatusDialog::OnInitDialog(UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam,
                                     BOOL& handled)
{
    DlgResize_Init();

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(small_icon_, TRUE);

    GetDlgItem(IDCANCEL).SetFocus();

    delegate_->OnStatusDialogOpen();

    DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return FALSE;
}

LRESULT UiStatusDialog::OnClose(UINT message,
                                WPARAM wparam,
                                LPARAM lparam,
                                BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiStatusDialog::OnCloseButton(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    EndDialog(0);
    return 0;
}

void UiStatusDialog::AddMessage(const CString& message)
{
    CEdit status_edit(GetDlgItem(IDC_STATUS_EDIT));

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

} // namespace aspia
