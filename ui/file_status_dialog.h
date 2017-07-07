//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_STATUS_DIALOG_H
#define _ASPIA_UI__FILE_STATUS_DIALOG_H

#include "base/message_loop/message_loop_thread.h"
#include "base/files/file_path.h"
#include "proto/file_transfer_session.pb.h"
#include "proto/status.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlframe.h>
#include <atlmisc.h>

namespace aspia {

class UiFileStatusDialog :
    public CDialogImpl<UiFileStatusDialog>,
    public CDialogResize<UiFileStatusDialog>,
    private MessageLoopThread::Delegate
{
public:
    enum { IDD = IDD_FILE_STATUS };

    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
    };

    UiFileStatusDialog(Delegate* delegate);
    ~UiFileStatusDialog();

    void SetDriveListRequestStatus(proto::Status status);

    void SetFileListRequestStatus(const FilePath& path, proto::Status status);

    void SetCreateDirectoryRequestStatus(const FilePath& path, proto::Status status);

    void SetRenameRequestStatus(const FilePath& old_name,
                                const FilePath& new_name,
                                proto::Status status);

    void SetRemoveRequestStatus(const FilePath& path, proto::Status status);

private:
    BEGIN_MSG_MAP(UiFileStatusDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_MINIMIZE_BUTTON, OnMinimizeButton)
        COMMAND_ID_HANDLER(IDC_STOP_BUTTON, OnStopButton)

        CHAIN_MSG_MAP(CDialogResize<UiFileStatusDialog>)
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

    void WriteLog(const CString& message, proto::Status status);

    Delegate* delegate_ = nullptr;

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiFileStatusDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_STATUS_DIALOG_H
