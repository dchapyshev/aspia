//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_status_dialog.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

#include <atlctrls.h>

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
    LoadLibraryW(L"msftedit.dll");

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

        // The default text limit is 64K characters.
        edit_ = GetDlgItem(IDC_STATUS_EDIT);
        edit_.LimitText(0xFFFFFFFF);
    }
}

void UiFileStatusDialog::OnAfterThreadRunning()
{
    DestroyWindow();
}

LRESULT UiFileStatusDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    DlgResize_Init();
    CenterWindow();

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

    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    GetDlgItem(IDC_MINIMIZE_BUTTON).SetFocus();
    return FALSE;
}

LRESULT UiFileStatusDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    PostQuitMessage(0);
    return 0;
}

LRESULT UiFileStatusDialog::OnMinimizeButton(WORD notify_code, WORD control_id,
                                             HWND control, BOOL& handled)
{
    ShowWindow(SW_MINIMIZE);
    return 0;
}

LRESULT UiFileStatusDialog::OnStopButton(WORD notify_code, WORD control_id,
                                         HWND control, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void UiFileStatusDialog::WriteMessage(const WCHAR* message)
{
    WCHAR time[128];

    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, time, _countof(time)))
    {
        std::wstring text = StringPrintfW(L"%s %s\r\n", time, message);

        SETTEXTEX settext;
        settext.codepage = 1200; // Unicode.
        settext.flags = ST_SELECTION;

        edit_.SetTextEx(&settext, text.c_str());
        edit_.SendMessageW(WM_VSCROLL, SB_BOTTOM);
    }
}

void UiFileStatusDialog::OnSessionStarted()
{
    CString message;
    message.LoadStringW(IDS_FT_OP_SESSION_START);
    WriteMessage(message);
}

void UiFileStatusDialog::OnSessionTerminated()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnSessionTerminated, this));
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
    WriteMessage(message);
}

void UiFileStatusDialog::OnDriveListRequest()
{
    CString message;
    message.LoadStringW(IDS_FT_OP_BROWSE_DRIVES);
    WriteMessage(message);
}

void UiFileStatusDialog::OnFileListRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_BROWSE_FOLDERS, path.c_str());
    WriteMessage(message);
}

void UiFileStatusDialog::OnCreateDirectoryRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_CREATE_FOLDER, path.c_str());
    WriteMessage(message);
}

void UiFileStatusDialog::OnRenameRequest(const FilePath& old_name, const FilePath& new_name)
{
    CString message;
    message.Format(IDS_FT_OP_RENAME, old_name.c_str(), new_name.c_str());
    WriteMessage(message);
}

void UiFileStatusDialog::OnRemoveRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_REMOVE, path.c_str());
    WriteMessage(message);
}

void UiFileStatusDialog::OnFileUploadRequest(const FilePath& file_path)
{
    CString message;
    message.Format(IDS_FT_OP_RECIEVE_FILE, file_path.c_str());
    WriteMessage(message);
}

void UiFileStatusDialog::OnFileDownloadRequest(const FilePath& file_path)
{
    CString message;
    message.Format(IDS_FT_OP_SEND_FILE, file_path.c_str());
    WriteMessage(message);
}

} // namespace aspia
