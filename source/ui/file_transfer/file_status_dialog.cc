//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer/file_status_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_status_dialog.h"
#include "base/files/base_paths.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

static const WCHAR kRichEditLibraryName[] = L"msftedit.dll";

FileStatusDialog::FileStatusDialog()
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

FileStatusDialog::~FileStatusDialog()
{
    ui_thread_.Stop();
}

void FileStatusDialog::WaitForClose()
{
    ui_thread_.Join();
}

void FileStatusDialog::OnBeforeThreadRunning()
{
    FilePath library_path;

    if (GetBasePath(BasePathKey::DIR_SYSTEM, library_path))
    {
        library_path.append(kRichEditLibraryName);

        // We need to load the library to work with RichEdit before creating a dialog.
        richedit_library_ = std::make_unique<ScopedNativeLibrary>(library_path.c_str());

        if (richedit_library_->IsLoaded())
        {
            runner_ = ui_thread_.message_loop_proxy();
            DCHECK(runner_);

            HWND window = Create(nullptr);
            if (window)
            {
                ShowWindow(SW_SHOWNORMAL);

                edit_ = GetDlgItem(IDC_STATUS_EDIT);
                // The default text limit is 64K characters. We increase this size.
                edit_.LimitText(0xFFFFFFFF);
                return;
            }
        }
    }

    runner_->PostQuit();
}

void FileStatusDialog::OnAfterThreadRunning()
{
    DestroyWindow();
}

LRESULT FileStatusDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

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
    SetIcon(big_icon_, TRUE);

    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    GetDlgItem(IDC_MINIMIZE_BUTTON).SetFocus();
    return FALSE;
}

LRESULT FileStatusDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    PostQuitMessage(0);
    return 0;
}

LRESULT FileStatusDialog::OnMinimizeButton(WORD notify_code, WORD control_id, HWND control,
                                           BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    ShowWindow(SW_MINIMIZE);
    return 0;
}

LRESULT FileStatusDialog::OnStopButton(WORD notify_code, WORD control_id, HWND control,
                                       BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    PostMessageW(WM_CLOSE);
    return 0;
}

void FileStatusDialog::WriteMessage(const WCHAR* message)
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

void FileStatusDialog::OnSessionStarted()
{
    CString message;
    message.LoadStringW(IDS_FT_OP_SESSION_START);
    WriteMessage(message);
}

void FileStatusDialog::OnSessionTerminated()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileStatusDialog::OnSessionTerminated, this));
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

void FileStatusDialog::OnDriveListRequest()
{
    CString message;
    message.LoadStringW(IDS_FT_OP_BROWSE_DRIVES);
    WriteMessage(message);
}

void FileStatusDialog::OnFileListRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_BROWSE_FOLDERS, path.c_str());
    WriteMessage(message);
}

void FileStatusDialog::OnCreateDirectoryRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_CREATE_FOLDER, path.c_str());
    WriteMessage(message);
}

void FileStatusDialog::OnRenameRequest(const FilePath& old_name, const FilePath& new_name)
{
    CString message;
    message.Format(IDS_FT_OP_RENAME, old_name.c_str(), new_name.c_str());
    WriteMessage(message);
}

void FileStatusDialog::OnRemoveRequest(const FilePath& path)
{
    CString message;
    message.Format(IDS_FT_OP_REMOVE, path.c_str());
    WriteMessage(message);
}

void FileStatusDialog::OnFileUploadRequest(const FilePath& file_path)
{
    CString message;
    message.Format(IDS_FT_OP_RECIEVE_FILE, file_path.c_str());
    WriteMessage(message);
}

void FileStatusDialog::OnFileDownloadRequest(const FilePath& file_path)
{
    CString message;
    message.Format(IDS_FT_OP_SEND_FILE, file_path.c_str());
    WriteMessage(message);
}

} // namespace aspia
