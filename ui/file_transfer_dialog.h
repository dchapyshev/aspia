//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER_DIALOG_H

#include "base/macros.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiFileTransferDialog : public CDialogImpl<UiFileTransferDialog>
{
public:
    enum { IDD = IDD_FILE_TRANSFER };

    UiFileTransferDialog() = default;
    ~UiFileTransferDialog() = default;

private:
    BEGIN_MSG_MAP(UiFileTransferDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    DISALLOW_COPY_AND_ASSIGN(UiFileTransferDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER_DIALOG_H
