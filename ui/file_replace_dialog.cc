//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_replace_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_replace_dialog.h"

namespace aspia {

LRESULT UiFileReplaceDialog::OnInitDialog(UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          BOOL& handled)
{
    // TODO
    return TRUE;
}

LRESULT UiFileReplaceDialog::OnClose(UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam,
                                     BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiFileReplaceDialog::OnCancelButton(WORD notify_code,
                                            WORD control_id,
                                            HWND control,
                                            BOOL& handled)
{
    EndDialog(0);
    return 0;
}

} // namespace aspia
