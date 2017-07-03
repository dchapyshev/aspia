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

void UiFileStatusDialog::SetRequestStatus(const proto::RequestStatus& status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetRequestStatus,
                                    this,
                                    status));
        return;
    }

    std::wstring first_path = UNICODEfromUTF8(status.first_path());
    std::wstring second_path = UNICODEfromUTF8(status.second_path());

    CString message;

    switch (status.type())
    {
        case proto::RequestStatus::DRIVE_LIST:
            message.LoadStringW(IDS_FT_OP_BROWSE_DRIVES);
            break;

        case proto::RequestStatus::DIRECTORY_LIST:
            message.Format(IDS_FT_OP_BROWSE_FOLDERS, first_path.c_str());
            break;

        case proto::RequestStatus::CREATE_DIRECTORY:
            message.Format(IDS_FT_OP_CREATE_FOLDER, first_path.c_str());
            break;

        case proto::RequestStatus::RENAME:
            message.Format(IDS_FT_OP_RENAME,
                           first_path.c_str(),
                           second_path.c_str());
            break;

        case proto::RequestStatus::REMOVE:
            message.Format(IDS_FT_OP_REMOVE, first_path.c_str());
            break;

        case proto::RequestStatus::SEND_FILE:
            message.Format(IDS_FT_OP_SEND_FILE, first_path.c_str());
            break;

        case proto::RequestStatus::RECIEVE_FILE:
            message.Format(IDS_FT_OP_RECIEVE_FILE, first_path.c_str());
            break;

        default:
            LOG(FATAL) << "Unhandled status code: " << status.type();
            break;
    }

    WriteLog(message, status.code());
}

} // namespace aspia
