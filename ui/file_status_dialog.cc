//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_status_dialog.h"
#include "ui/status_code.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

#include <filesystem>

namespace aspia {

namespace fs = std::experimental::filesystem;

static const int kBorderSize = 10;

UiFileStatusDialog::UiFileStatusDialog(Delegate* delegate) :
    delegate_(delegate)
{
    DCHECK(delegate_);

    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

UiFileStatusDialog::~UiFileStatusDialog()
{
    ui_thread_.Stop();
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
    delegate_->OnWindowClose();
}

LRESULT UiFileStatusDialog::OnInitDialog(UINT message,
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

    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    CString start_message;
    start_message.LoadStringW(IDS_FT_OP_SESSION_START);

    WriteLog(start_message, proto::Status::STATUS_SUCCESS);

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

static std::wstring CurrentTime()
{
    WCHAR buffer[128];

    if (!GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
        return std::wstring();

    std::wstring datetime(buffer);

    if (!GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
        return std::wstring();

    datetime.append(L" ");
    datetime.append(buffer);

    return datetime;
}

void UiFileStatusDialog::WriteLog(const CString& message, proto::Status status)
{
    CEdit edit(GetDlgItem(IDC_STATUS_EDIT));

    edit.AppendText(CurrentTime().c_str());
    edit.AppendText(L" ");
    edit.AppendText(message);

    if (status != proto::Status::STATUS_SUCCESS)
    {
        edit.AppendText(L" (");
        edit.AppendText(StatusCodeToString(status));
        edit.AppendText(L")");
    }

    edit.AppendText(L"\r\n");
}

void UiFileStatusDialog::SetDriveListRequestStatus(proto::Status status)
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
                                                  proto::Status status)
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
                                                         proto::Status status)
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
                                                proto::Status status)
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
                                                proto::Status status)
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

} // namespace aspia
