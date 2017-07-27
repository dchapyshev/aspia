//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_status_dialog.h"
#include "ui/status_code.h"
#include "base/logging.h"

#include <atlctrls.h>
#include <chrono>
#include <clocale>

namespace aspia {

UiFileStatusDialog::UiFileStatusDialog()
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

UiFileStatusDialog::~UiFileStatusDialog()
{
    ui_thread_.Stop();
}

void UiFileStatusDialog::WaitForClose()
{
    ui_thread_.Join();
}

void UiFileStatusDialog::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    HWND window = Create(nullptr);
    if (!window)
    {
        LOG(ERROR) << "File status dialog not created";
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOWNORMAL);
    }
}

void UiFileStatusDialog::OnAfterThreadRunning()
{
    DestroyWindow();
}

LRESULT UiFileStatusDialog::OnInitDialog(UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL& handled)
{
    DlgResize_Init();

    // Disable close button for dialog.
    DWORD style = GetClassLongPtrW(*this, GCL_STYLE);
    SetClassLongPtrW(*this, GCL_STYLE, style | CS_NOCLOSE);

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

    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    GetDlgItem(IDC_MINIMIZE_BUTTON).SetFocus();
    return FALSE;
}

LRESULT UiFileStatusDialog::OnClose(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    PostQuitMessage(0);
    return 0;
}

LRESULT UiFileStatusDialog::OnMinimizeButton(WORD notify_code,
                                             WORD control_id,
                                             HWND control,
                                             BOOL& handled)
{
    ShowWindow(SW_MINIMIZE);
    return 0;
}

LRESULT UiFileStatusDialog::OnStopButton(WORD notify_code,
                                         WORD control_id,
                                         HWND control,
                                         BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

static std::wstring GetCurrentDateTime()
{
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    std::time_t time = std::chrono::system_clock::to_time_t(now);

    tm* local_time = std::localtime(&time);
    if (local_time)
    {
        // Set the locale obtained from system.
        std::setlocale(LC_TIME, "");

        WCHAR string[128];
        if (std::wcsftime(string, _countof(string), L"%x %X", local_time))
        {
            return string;
        }
    }

    return std::wstring();
}

void UiFileStatusDialog::WriteLog(const CString& message, proto::RequestStatus status)
{
    CEdit edit(GetDlgItem(IDC_STATUS_EDIT));

    edit.AppendText(GetCurrentDateTime().c_str());
    edit.AppendText(L" ");
    edit.AppendText(message);

    if (status != proto::RequestStatus::REQUEST_STATUS_SUCCESS)
    {
        edit.AppendText(L" (");
        edit.AppendText(RequestStatusCodeToString(status));
        edit.AppendText(L")");
    }

    edit.AppendText(L"\r\n");
}

void UiFileStatusDialog::SetSessionStartedStatus()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetSessionStartedStatus, this));
        return;
    }

    CString message;
    message.LoadStringW(IDS_FT_OP_SESSION_START);
    WriteLog(message, proto::RequestStatus::REQUEST_STATUS_SUCCESS);
}

void UiFileStatusDialog::SetSessionTerminatedStatus()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetSessionTerminatedStatus, this));
        return;
    }

    // Enable close button for dialog.
    DWORD style = GetClassLongPtrW(*this, GCL_STYLE);
    SetClassLongPtrW(*this, GCL_STYLE, style & ~CS_NOCLOSE);

    CString button_title;
    button_title.LoadStringW(IDS_FT_CLOSE_WINDOW);
    GetDlgItem(IDC_STOP_BUTTON).SetWindowTextW(button_title);

    CString message;
    message.LoadStringW(IDS_FT_OP_SESSION_END);
    WriteLog(message, proto::RequestStatus::REQUEST_STATUS_SUCCESS);
}

void UiFileStatusDialog::SetDriveListRequestStatus(proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetDriveListRequestStatus,
                                    this,
                                    status));
        return;
    }

    CString message;
    message.LoadStringW(IDS_FT_OP_BROWSE_DRIVES);
    WriteLog(message, status);
}

void UiFileStatusDialog::SetFileListRequestStatus(const FilePath& path,
                                                  proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetFileListRequestStatus,
                                    this,
                                    path,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_BROWSE_FOLDERS, path.c_str());
    WriteLog(message, status);
}

void UiFileStatusDialog::SetCreateDirectoryRequestStatus(const FilePath& path,
                                                         proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetCreateDirectoryRequestStatus,
                                    this,
                                    path,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_CREATE_FOLDER, path.c_str());
    WriteLog(message, status);
}

void UiFileStatusDialog::SetRenameRequestStatus(const FilePath& old_name,
                                                const FilePath& new_name,
                                                proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetRenameRequestStatus,
                                    this,
                                    old_name,
                                    new_name,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_RENAME, old_name.c_str(), new_name.c_str());
    WriteLog(message, status);
}

void UiFileStatusDialog::SetRemoveRequestStatus(const FilePath& path,
                                                proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetRemoveRequestStatus,
                                    this,
                                    path,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_REMOVE, path.c_str());
    WriteLog(message, status);
}

void UiFileStatusDialog::SetFileUploadRequestStatus(const FilePath& file_path,
                                                    proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetFileUploadRequestStatus,
                                    this,
                                    file_path,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_RECIEVE_FILE, file_path.c_str());
    WriteLog(message, status);
}

void UiFileStatusDialog::SetFileDownloadRequestStatus(const FilePath& file_path,
                                                      proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetFileDownloadRequestStatus,
                                    this,
                                    file_path,
                                    status));
        return;
    }

    CString message;
    message.Format(IDS_FT_OP_SEND_FILE, file_path.c_str());
    WriteLog(message, status);
}

} // namespace aspia
