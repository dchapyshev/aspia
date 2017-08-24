//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer/file_status_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_DIALOG_H

#include "base/message_loop/message_loop_thread.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlframe.h>

namespace aspia {

class FileStatusDialog :
    public CDialogImpl<FileStatusDialog>,
    public CDialogResize<FileStatusDialog>,
    private MessageLoopThread::Delegate
{
public:
    enum { IDD = IDD_FILE_STATUS };

    FileStatusDialog();
    ~FileStatusDialog();

    void WaitForClose();

    void OnSessionStarted();
    void OnSessionTerminated();
    void OnDriveListRequest();
    void OnFileListRequest(const FilePath& path);
    void OnCreateDirectoryRequest(const FilePath& path);
    void OnRenameRequest(const FilePath& old_name, const FilePath& new_name);
    void OnRemoveRequest(const FilePath& path);
    void OnFileUploadRequest(const FilePath& file_path);
    void OnFileDownloadRequest(const FilePath& file_path);

private:
    BEGIN_MSG_MAP(FileStatusDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_MINIMIZE_BUTTON, OnMinimizeButton)
        COMMAND_ID_HANDLER(IDC_STOP_BUTTON, OnStopButton)

        CHAIN_MSG_MAP(CDialogResize<FileStatusDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(UiFileStatusDialog)
        DLGRESIZE_CONTROL(IDC_STATUS_EDIT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDC_STOP_BUTTON, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDC_MINIMIZE_BUTTON, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMinimizeButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnStopButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void WriteMessage(const WCHAR* text);

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    std::unique_ptr<ScopedNativeLibrary> richedit_library_;

    CIcon small_icon_;
    CIcon big_icon_;
    CRichEditCtrl edit_;

    DISALLOW_COPY_AND_ASSIGN(FileStatusDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__FILE_STATUS_DIALOG_H
