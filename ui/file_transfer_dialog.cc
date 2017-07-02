//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer_dialog.h"

namespace aspia {

LRESULT UiFileTransferDialog::OnInitDialog(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL& handled)
{
    // TODO
    return TRUE;
}

LRESULT UiFileTransferDialog::OnClose(UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiFileTransferDialog::OnCancelButton(WORD notify_code,
                                             WORD control_id,
                                             HWND control,
                                             BOOL& handled)
{
    EndDialog(0);
    return 0;
}

} // namespace aspia
