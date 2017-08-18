//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_remove_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_REMOVE_DIALOG_H
#define _ASPIA_UI__FILE_REMOVE_DIALOG_H

#include "base/message_loop/message_loop_proxy.h"
#include "client/file_reply_receiver.h"
#include "client/file_request_sender_proxy.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class FileRemoveDialog :
    public CDialogImpl<FileRemoveDialog>
{
public:
    enum { IDD = IDD_FILE_PROGRESS };

    FileRemoveDialog(std::shared_ptr<FileRequestSenderProxy> sender);
    ~FileRemoveDialog() = default;

private:
    BEGIN_MSG_MAP(FileRemoveDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    std::shared_ptr<MessageLoopProxy> runner_;
    std::shared_ptr<FileRequestSenderProxy> sender_;

    CProgressBarCtrl total_progress_;
    CProgressBarCtrl current_progress_;
    CEdit current_item_edit_;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_REMOVE_DIALOG_H
