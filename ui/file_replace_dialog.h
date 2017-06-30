//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_replace_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_REPLACE_DIALOG_H
#define _ASPIA_UI__FILE_REPLACE_DIALOG_H

#include "base/macros.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiFileReplaceDialog : public CDialogImpl<UiFileReplaceDialog>
{
public:
    enum { IDD = IDD_FILE_REPLACE };

    UiFileReplaceDialog() = default;
    ~UiFileReplaceDialog() = default;

private:
    BEGIN_MSG_MAP(UiFileReplaceDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    DISALLOW_COPY_AND_ASSIGN(UiFileReplaceDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_REPLACE_DIALOG_H
